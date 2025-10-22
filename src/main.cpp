// --- インクルード ---
#include "network.h"          // ネットワーク通信関連 (UDP送受信)
#include "gamepad.h"          // ゲームパッドデータ構造体とパース関数
#include "thruster_control.h" // スラスター制御関連
#include "sensor_data.h"      // センサーデータ読み取り・フォーマット関連
#include "gstPipeline.h"      // GStreamerパイプライン起動用
#include "config.h"           // 設定ファイル読み込みとグローバル設定オブジェクト
#include "config_synchronizer.h" // 設定同期用

#include <iostream> // 標準入出力 (std::cout, std::cerr)
#include <unistd.h> // POSIX API (usleep)
#include <string.h> // strlen
#include <sys/time.h> // gettimeofday
#include <csignal> // シグナルハンドリング用
#include <mutex>    // std::mutex, std::lock_guard

// --- グローバル変数 ---
// AppConfig g_config; // config.cpp で定義
// std::mutex g_config_mutex; // config.cpp で定義

// --- メイン関数 ---
int main()
{
    printf("Navigator C++ Control Application\n");
    // --- 設定ファイルの読み込み ---
    if (!loadConfig("config.ini")) {
        std::cerr << "致命的エラー: 設定ファイルの初期読み込みに失敗しました。プログラムを終了します。" << std::endl;
        return -1;
    }

    // --- 設定同期スレッドの開始 ---
    ConfigSynchronizer config_sync("config.ini");
    config_sync.start();

    // --- 初期化 ---
    printf("Initiating navigator module.\n");
    init(); // Navigator ハードウェアライブラリの初期化 (bindings.h 経由)

    NetworkContext net_ctx;
    if (!network_init(&net_ctx))
    {
        std::cerr << "ネットワーク初期化失敗。終了します。" << std::endl;
        return -1;
    }

    if (!thruster_init())
    {
        std::cerr << "スラスター初期化失敗。終了します。" << std::endl;
        network_close(&net_ctx);
        return -1;
    }

    if (!start_gstreamer_pipelines())
    {
        std::cerr << "GStreamerパイプラインの起動に失敗しました。処理を続行します..." << std::endl;
    }

    // --- メインループ変数 ---
    GamepadData latest_gamepad_data;
    char recv_buffer[NET_BUFFER_SIZE];
    AxisData current_gyro_data = {0.0f, 0.0f, 0.0f};
    char sensor_buffer[SENSOR_BUFFER_SIZE];
    unsigned int loop_counter = 0;
    bool running = true;
    bool currently_in_failsafe = true;

    int initial_pwm_min;
    {
        std::lock_guard<std::mutex> lock(g_config_mutex);
        initial_pwm_min = g_config.pwm_min;
    }
    std::cout << "メインループ開始。" << std::endl;
    std::cout << "クライアントからの最初のデータ受信を待機しています... (スラスターはPWM: " << initial_pwm_min << ")" << std::endl;
    thruster_set_all_pwm(initial_pwm_min);

    while (running)
    {
        // --- 設定のローカルコピーを取得 ---
        double current_connection_timeout;
        unsigned int current_sensor_send_interval;
        unsigned int current_loop_delay_us;
        int current_pwm_min;

        {
            std::lock_guard<std::mutex> lock(g_config_mutex);
            current_connection_timeout = g_config.connection_timeout_seconds;
            current_sensor_send_interval = g_config.sensor_send_interval;
            current_loop_delay_us = g_config.loop_delay_us;
            current_pwm_min = g_config.pwm_min;
        }

        // 設定ファイルが外部から更新されたかチェックし、リロードする
        if (g_config_updated_flag.load()) {
            std::cout << "設定ファイルが更新されました。リロードします..." << std::endl;
            if (!loadConfig("config.ini")) {
                std::cerr << "警告: 設定ファイルのリロードに失敗しました。古い設定で動作を継続します。" << std::endl;
            }
            g_config_updated_flag.store(false); // フラグをリセット
        }

        struct timeval current_time_tv;
        gettimeofday(&current_time_tv, NULL);

        double time_since_last_packet = 0.0;
        if (net_ctx.client_addr_known)
        {
            time_since_last_packet = (current_time_tv.tv_sec - net_ctx.last_successful_recv_time.tv_sec) +
                                     (current_time_tv.tv_usec - net_ctx.last_successful_recv_time.tv_usec) / 1000000.0;
        }

        ssize_t recv_len = network_receive(&net_ctx, recv_buffer, sizeof(recv_buffer));
        bool just_received_packet = (recv_len > 0);

        if (just_received_packet)
        {
            if (currently_in_failsafe)
            {
                std::cout << "接続確立/再確立。通常動作を再開します。" << std::endl;
                currently_in_failsafe = false;
            }
            std::string received_str(recv_buffer, recv_len);
            latest_gamepad_data = parseGamepadData(received_str);
        }
        else
        {
            if (net_ctx.client_addr_known && time_since_last_packet > current_connection_timeout)
            {
                if (!currently_in_failsafe)
                {
                    std::cout << "接続がタイムアウトしました。フェイルセーフモード (スラスターPWM: " << current_pwm_min << ") に移行します。" << std::endl;
                    thruster_set_all_pwm(current_pwm_min);
                    latest_gamepad_data = GamepadData{};
                    currently_in_failsafe = true;
                    // GStreamerのクリーンな再確立のため、プロセスを終了してsystemdによる再起動に任せる
                    std::cout << "フェイルセーフ起動のためプログラムを終了します。" << std::endl;
                    running = false;
                }
            }
            if (recv_len < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                std::cerr << "致命的な受信エラー。ループを継続します..." << std::endl;
            }
        }

        if (!currently_in_failsafe && running)
        {
            current_gyro_data = read_gyro();
            thruster_update(latest_gamepad_data, current_gyro_data);

            if (loop_counter >= current_sensor_send_interval)
            {
                loop_counter = 0;
                if (read_and_format_sensor_data(sensor_buffer, sizeof(sensor_buffer)))
                {
                    std::cout << "[SENSOR LOG] " << sensor_buffer << std::endl;
                    network_send(&net_ctx, sensor_buffer, strlen(sensor_buffer));
                }
                else
                {
                    std::cerr << "センサーデータの読み取り/フォーマットに失敗。" << std::endl;
                }
            }
            else
            {
                loop_counter++;
            }
        }
        else
        {
            loop_counter = 0;
        }

        usleep(current_loop_delay_us);
    }

    // --- クリーンアップ ---
    std::cout << "クリーンアップ処理を開始します..." << std::endl;
    config_sync.stop();
    std::cout << "設定同期スレッドを停止しました..." << std::endl;
    
    int final_pwm_min;
    {
        std::lock_guard<std::mutex> lock(g_config_mutex);
        final_pwm_min = g_config.pwm_min;
    }
    thruster_set_all_pwm(final_pwm_min); // 最後に安全な値に設定
    thruster_disable();
    std::cout << "PWMの出力を停止しました..." << std::endl;
    network_close(&net_ctx);
    std::cout << "ネットワークをクローズしました..." << std::endl;
    stop_gstreamer_pipelines();
    std::cout << "Gstreamerパイプラインを停止しました..." << std::endl;
    std::cout << "プログラム終了。" << std::endl;
    return 0;
}

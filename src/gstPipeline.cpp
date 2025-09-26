#include "gstPipeline.h"
#include <iostream>
#include <string>   // std::stringとstd::to_stringのため
#include <thread>   // std::threadのため
#include "config.h" // g_config を使用するため

// --- グローバル変数 ---
// GStreamerパイプラインのインスタンス (カメラ1用)
static GstElement *pipeline1 = nullptr; 
// GStreamerパイプラインのインスタンス (カメラ2用)
static GstElement *pipeline2 = nullptr; 
// GStreamerのメインループ (カメラ1用)。イベント処理やメッセージ処理を行う。
static GMainLoop *main_loop1 = nullptr; 
// GStreamerのメインループ (カメラ2用)
static GMainLoop *main_loop2 = nullptr; 
// main_loop1を実行するためのスレッド
static std::thread loop_thread1; 
// main_loop2を実行するためのスレッド
static std::thread loop_thread2; 


// GMainLoopを指定されたスレッドで実行するための関数
static void run_main_loop(GMainLoop* loop) {
    g_main_loop_run(loop);
}

static bool create_pipeline(const AppConfig& app_config, int camera_idx, GstElement** pipeline_ptr, GMainLoop** loop_ptr) {
    std::string device;
    int port;
    const std::string& host = app_config.client_host; // 共通のホストIPを使用
    int width;
    int height;
    int framerate_num;
    int framerate_den;
    bool is_h264_native_source;
    int rtp_payload_type;
    int rtp_config_interval;
    int x264_bitrate = 0; // カメラ2のみ
    std::string x264_tune; // カメラ2のみ
    std::string x264_speed_preset; // カメラ2のみ

    if (camera_idx == 1) {
        device = app_config.gst1_device;
        port = app_config.gst1_port;
        width = app_config.gst1_width;
        height = app_config.gst1_height;
        framerate_num = app_config.gst1_framerate_num;
        framerate_den = app_config.gst1_framerate_den;
        is_h264_native_source = app_config.gst1_is_h264_native_source;
        rtp_payload_type = app_config.gst1_rtp_payload_type;
        rtp_config_interval = app_config.gst1_rtp_config_interval;
    } else if (camera_idx == 2) {
        device = app_config.gst2_device;
        port = app_config.gst2_port;
        width = app_config.gst2_width;
        height = app_config.gst2_height;
        framerate_num = app_config.gst2_framerate_num;
        framerate_den = app_config.gst2_framerate_den;
        is_h264_native_source = app_config.gst2_is_h264_native_source;
        rtp_payload_type = app_config.gst2_rtp_payload_type;
        rtp_config_interval = app_config.gst2_rtp_config_interval;
        x264_bitrate = app_config.gst2_x264_bitrate;
        x264_tune = app_config.gst2_x264_tune;
        x264_speed_preset = app_config.gst2_x264_speed_preset;
    } else {
        std::cerr << "エラー: 不明なカメラインデックス " << camera_idx << std::endl;
        return false;
    }

    std::string pipeline_str = "v4l2src device=" + device + " ! ";

    if (is_h264_native_source) {
        // カメラがH.264ネイティブ出力の場合のパイプライン文字列を構築
        // v4l2src -> video/x-h264 caps -> h264parse
        pipeline_str += "video/x-h264,width=" + std::to_string(width) +
                        ",height=" + std::to_string(height) +
                        ",framerate=" + std::to_string(framerate_num) + "/" + std::to_string(framerate_den) + " ! "
                        "h264parse config-interval=" + std::to_string(rtp_config_interval);
    } else {
        // カメラがJPEG出力など、H.264へのエンコードが必要な場合のパイプライン文字列を構築
        // v4l2src -> image/jpeg caps -> jpegdec -> videoconvert -> x264enc
        pipeline_str += "image/jpeg,width=" + std::to_string(width) +
                        ",height=" + std::to_string(height) +
                        ",framerate=" + std::to_string(framerate_num) + "/" + std::to_string(framerate_den) + " ! "
                        "jpegdec ! videoconvert ! "
                        "x264enc tune=" + x264_tune +
                        " bitrate=" + std::to_string(x264_bitrate) +
                        " speed-preset=" + x264_speed_preset;
    }

    // 共通のパイプライン末尾部分 (RTPパッキングとUDP送信) を追加
    // ... ! rtph264pay ! udpsink
    pipeline_str += " ! rtph264pay config-interval=" + std::to_string(rtp_config_interval) +
                    " pt=" + std::to_string(rtp_payload_type) + " ! "
                    "udpsink host=" + host + " port=" + std::to_string(port);

    GError* error = nullptr;
    // 構築したパイプライン文字列からGStreamerパイプラインをパース(作成)
    *pipeline_ptr = gst_parse_launch(pipeline_str.c_str(), &error);
    if (!*pipeline_ptr) {
        // パイプライン作成失敗時のエラー処理
        std::cerr << "GStreamerパイプライン作成失敗 (カメラ" << camera_idx << " - " << device << "): " << error->message << std::endl;
        g_error_free(error);
        return false;
    }
    // 作成されたパイプライン文字列をデバッグ出力
    std::cout << "GStreamer pipeline for camera " << camera_idx << " (" << device << "): " << pipeline_str << std::endl;

    // パイプライン用のGMainLoopを作成
    *loop_ptr = g_main_loop_new(nullptr, FALSE);
    // パイプラインをPLAYING状態に遷移させる
    gst_element_set_state(*pipeline_ptr, GST_STATE_PLAYING);

    return true;
}

// GStreamerパイプラインを開始するメイン関数
bool start_gstreamer_pipelines() {
    // GStreamerライブラリの初期化 (アプリケーション開始時に一度だけ呼び出す)
    gst_init(nullptr, nullptr);


    // カメラ1のパイプラインを作成・起動
    if (!create_pipeline(g_config, 1, &pipeline1, &main_loop1)) return false;

    // カメラ2のパイプラインを作成・起動
    if (!create_pipeline(g_config, 2, &pipeline2, &main_loop2)) return false;

    // 各パイプラインのGMainLoopを別々のスレッドで実行開始
    loop_thread1 = std::thread(run_main_loop, main_loop1);
    loop_thread2 = std::thread(run_main_loop, main_loop2);

    std::cout << "GStreamerパイプラインを非同期で起動しました。" << std::endl;
    return true;
}
// GStreamerパイプラインを停止し、リソースを解放する関数
void stop_gstreamer_pipelines() {
    std::cout << "GStreamerパイプラインを停止します..." << std::endl;

    if (pipeline1) {
        // パイプライン1をNULL状態に遷移させて停止
        gst_element_set_state(pipeline1, GST_STATE_NULL);
        // パイプライン1オブジェクトの参照カウントを減らす (不要になれば解放される)
        gst_object_unref(pipeline1);
        pipeline1 = nullptr;
    }
    if (main_loop1) {
        // メインループ1に終了を要求
        g_main_loop_quit(main_loop1);
        // メインループ1を実行しているスレッドが終了するのを待つ
        if (loop_thread1.joinable()) loop_thread1.join();
        // メインループ1オブジェクトの参照カウントを減らす
        g_main_loop_unref(main_loop1);
        main_loop1 = nullptr;
    }

    if (pipeline2) {
        // パイプライン2をNULL状態に遷移させて停止
        gst_element_set_state(pipeline2, GST_STATE_NULL);
        // パイプライン2オブジェクトの参照カウントを減らす
        gst_object_unref(pipeline2);
        pipeline2 = nullptr;
    }
    if (main_loop2) {
        // メインループ2に終了を要求
        g_main_loop_quit(main_loop2);
        // メインループ2を実行しているスレッドが終了するのを待つ
        if (loop_thread2.joinable()) loop_thread2.join();
        // メインループ2オブジェクトの参照カウントを減らす
        g_main_loop_unref(main_loop2);
        main_loop2 = nullptr;
    }

    std::cout << "GStreamerパイプラインを停止しました。" << std::endl;
}

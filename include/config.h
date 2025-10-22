#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include <iostream>
#include <mutex> // std::mutex をインクルード

// 設定値を保持する構造体
struct AppConfig {
    // PWM設定
    int pwm_min;
    int pwm_neutral;
    int pwm_normal_max;
    int pwm_boost_max;
    float pwm_frequency;

    // ジョイスティック設定
    int joystick_deadzone;

    // LED設定
    int led_pwm_channel;
    int led_pwm_on;
    int led_pwm_off;

    // LED 2 (Bボタン) 設定
    int led2_pwm_channel;
    int led2_pwm_off;
    int led2_pwm_on1;
    int led2_pwm_on2;
    int led2_pwm_max;

    // LED 3 (Xボタン) 設定
    int led3_pwm_channel;
    int led3_pwm_off;
    int led3_pwm_on1;
    int led3_pwm_on2;
    int led3_pwm_max;

    // LED 4 (Aボタン) 設定
    int led4_pwm_channel;
    int led4_pwm_off;
    int led4_pwm_on1;
    int led4_pwm_on2;
    int led4_pwm_max;

    // LED 5 (RBボタン) 設定
    int led5_pwm_channel;
    int led5_pwm_off;
    int led5_pwm_on1;
    int led5_pwm_on2;
    int led5_pwm_max;

    // スラスター制御設定 (平滑化、ジャイロ補正)
    float smoothing_factor_horizontal;
    float smoothing_factor_vertical;
    float kp_roll;
    float kp_yaw;
    float yaw_threshold_dps;
    float yaw_gain;

    // ネットワーク設定
    int network_recv_port;
    int network_send_port;
    std::string client_host; // 操縦PC/GStreamer受信先のIPアドレス
    double connection_timeout_seconds;

    // アプリケーション設定
    unsigned int sensor_send_interval;
    unsigned int loop_delay_us; // usleep の引数

    // GStreamer カメラ1設定
    std::string gst1_device;
    int gst1_port;
    int gst1_width;
    int gst1_height;
    int gst1_framerate_num;
    int gst1_framerate_den;
    bool gst1_is_h264_native_source;
    int gst1_rtp_payload_type;
    int gst1_rtp_config_interval;

    // GStreamer カメラ2設定
    std::string gst2_device;
    int gst2_port;
    int gst2_width;
    int gst2_height;
    int gst2_framerate_num;
    int gst2_framerate_den;
    bool gst2_is_h264_native_source;
    int gst2_rtp_payload_type;
    int gst2_rtp_config_interval;
    int gst2_x264_bitrate;
    std::string gst2_x264_tune;
    std::string gst2_x264_speed_preset;

    // Config Synchronizer settings
    int config_sync_cpp_recv_port;
    std::string config_sync_wpf_host;
    int config_sync_wpf_recv_port;

    // デフォルト値を設定するコンストラクタ
    AppConfig(); // 実装は config.cpp に記述
};

// グローバル設定オブジェクト
extern AppConfig g_config;
// g_config を保護するためのグローバルミューテックス
extern std::mutex g_config_mutex;

// 設定ファイルを読み込む関数
bool loadConfig(const std::string& filename);

#endif // CONFIG_H
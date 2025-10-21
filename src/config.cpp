#include "config.h"
#include <fstream>
#include <sstream>
#include <algorithm> // for std::transform
#include <cctype>    // for std::isspace
#include <atomic>    // for std::atomic

// グローバル設定オブジェクトの実体
AppConfig g_config;

// 設定ファイルが更新されたことをメインスレッドに通知するためのフラグ
std::atomic<bool> g_config_updated_flag(false);

// AppConfig コンストラクタの実装 (デフォルト値の設定)
AppConfig::AppConfig() :
    pwm_min(1100), pwm_neutral(1500), pwm_normal_max(1900), pwm_boost_max(1900), pwm_frequency(50.0f),
    joystick_deadzone(6500),
    led_pwm_channel(9), led_pwm_on(1900), led_pwm_off(1100),
    led2_pwm_channel(10), led2_pwm_off(1100), led2_pwm_on1(1300), led2_pwm_on2(1600), led2_pwm_max(1900),
    led3_pwm_channel(11), led3_pwm_off(1100), led3_pwm_on1(1300), led3_pwm_on2(1600), led3_pwm_max(1900),
    led4_pwm_channel(12), led4_pwm_off(1100), led4_pwm_on1(1300), led4_pwm_on2(1600), led4_pwm_max(1900),
    led5_pwm_channel(13), led5_pwm_off(1100), led5_pwm_on1(1300), led5_pwm_on2(1600), led5_pwm_max(1900),
    smoothing_factor_horizontal(0.08f), smoothing_factor_vertical(0.04f),
    kp_roll(0.2f), kp_yaw(0.15f), yaw_threshold_dps(0.5f), yaw_gain(1000.0f),
    network_recv_port(12345), network_send_port(12346), client_host("192.168.4.10"), connection_timeout_seconds(0.2),
    sensor_send_interval(10), loop_delay_us(10000),
    gst1_device("/dev/video2"), gst1_port(5000),
    gst1_width(1280), gst1_height(720), gst1_framerate_num(30), gst1_framerate_den(1),
    gst1_is_h264_native_source(true), gst1_rtp_payload_type(96), gst1_rtp_config_interval(1),
    gst2_device("/dev/video4"), gst2_port(5001),
    gst2_width(1280), gst2_height(720), gst2_framerate_num(30), gst2_framerate_den(1),
    gst2_is_h264_native_source(false), gst2_rtp_payload_type(96), gst2_rtp_config_interval(1),
    gst2_x264_bitrate(5000), gst2_x264_tune("zerolatency"), gst2_x264_speed_preset("superfast"),
    config_sync_cpp_recv_port(12348), config_sync_wpf_host("192.168.4.10"), config_sync_wpf_recv_port(12347)
{}

// ヘルパー関数: 文字列の前後の空白を削除
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r\f\v");
    if (std::string::npos == first) return str;
    size_t last = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(first, (last - first + 1));
}

// ヘルパー関数: 文字列を小文字に変換
static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<unsigned char>(std::tolower(c)); });
    return s;
}

bool loadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "エラー: 設定ファイル '" << filename << "' を開けません。デフォルト値を使用します。" << std::endl;
        return false;
    }

    std::string line;
    std::string current_section;
    int line_num = 0;

    while (std::getline(file, line)) {
        line_num++;
        line = trim(line);

        if (line.empty() || line[0] == '#' || line[0] == ';') {
            // 空行またはコメント行
            continue;
        }

        if (line[0] == '[' && line.back() == ']') {
            // セクションヘッダー
            current_section = toLower(line.substr(1, line.length() - 2));
            continue;
        }

        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            std::cerr << "警告: " << filename << " の " << line_num << " 行目: '=' が見つかりません。スキップします。" << std::endl;
            continue;
        }

        std::string key = toLower(trim(line.substr(0, eq_pos)));
        std::string value = trim(line.substr(eq_pos + 1));

        // 設定値のパースと適用
        try {
            if (current_section == "pwm") {
                if (key == "pwm_min") g_config.pwm_min = std::stoi(value);
                else if (key == "pwm_neutral") g_config.pwm_neutral = std::stoi(value);
                else if (key == "pwm_normal_max") g_config.pwm_normal_max = std::stoi(value);
                else if (key == "pwm_boost_max") g_config.pwm_boost_max = std::stoi(value);
                else if (key == "pwm_frequency") g_config.pwm_frequency = std::stof(value);
            } else if (current_section == "joystick") {
                if (key == "deadzone") g_config.joystick_deadzone = std::stoi(value);
            } else if (current_section == "led") {
                if (key == "channel") g_config.led_pwm_channel = std::stoi(value);
                else if (key == "on_value") g_config.led_pwm_on = std::stoi(value);
                else if (key == "off_value") g_config.led_pwm_off = std::stoi(value);
            } else if (current_section == "led2") {
                if (key == "channel") g_config.led2_pwm_channel = std::stoi(value);
                else if (key == "off_value") g_config.led2_pwm_off = std::stoi(value);
                else if (key == "on1_value") g_config.led2_pwm_on1 = std::stoi(value);
                else if (key == "on2_value") g_config.led2_pwm_on2 = std::stoi(value);
                else if (key == "max_value") g_config.led2_pwm_max = std::stoi(value);
            } else if (current_section == "led3") {
                if (key == "channel") g_config.led3_pwm_channel = std::stoi(value);
                else if (key == "off_value") g_config.led3_pwm_off = std_::stoi(value);
                else if (key == "on1_value") g_config.led3_pwm_on1 = std::stoi(value);
                else if (key == "on2_value") g_config.led3_pwm_on2 = std::stoi(value);
                else if (key == "max_value") g_config.led3_pwm_max = std::stoi(value);
            } else if (current_section == "led4") {
                if (key == "channel") g_config.led4_pwm_channel = std::stoi(value);
                else if (key == "off_value") g_config.led4_pwm_off = std::stoi(value);
                else if (key == "on1_value") g_config.led4_pwm_on1 = std::stoi(value);
                else if (key == "on2_value") g_config.led4_pwm_on2 = std::stoi(value);
                else if (key == "max_value") g_config.led4_pwm_max = std::stoi(value);
            } else if (current_section == "led5") {
                if (key == "channel") g_config.led5_pwm_channel = std::stoi(value);
                else if (key == "off_value") g_config.led5_pwm_off = std::stoi(value);
                else if (key == "on1_value") g_config.led5_pwm_on1 = std::stoi(value);
                else if (key == "on2_value") g_config.led5_pwm_on2 = std::stoi(value);
                else if (key == "max_value") g_config.led5_pwm_max = std::stoi(value);
            } else if (current_section == "thruster_control") {
                if (key == "smoothing_factor_horizontal") g_config.smoothing_factor_horizontal = std::stof(value);
                else if (key == "smoothing_factor_vertical") g_config.smoothing_factor_vertical = std::stof(value);
                else if (key == "kp_roll") g_config.kp_roll = std::stof(value);
                else if (key == "kp_yaw") g_config.kp_yaw = std::stof(value);
                else if (key == "yaw_threshold_dps") g_config.yaw_threshold_dps = std::stof(value);
                else if (key == "yaw_gain") g_config.yaw_gain = std::stof(value);
            } else if (current_section == "network") {
                if (key == "recv_port") g_config.network_recv_port = std::stoi(value);
                else if (key == "send_port") g_config.network_send_port = std::stoi(value);
                else if (key == "client_host") g_config.client_host = value;
                else if (key == "connection_timeout_seconds") g_config.connection_timeout_seconds = std::stod(value);
            } else if (current_section == "application") {
                if (key == "sensor_send_interval") g_config.sensor_send_interval = std::stoul(value);
                else if (key == "loop_delay_us") g_config.loop_delay_us = std::stoul(value);
            } else if (current_section == "gstreamer_camera_1") {
                if (key == "port") g_config.gst1_port = std::stoi(value);
                else if (key == "width") g_config.gst1_width = std::stoi(value);
                else if (key == "height") g_config.gst1_height = std::stoi(value); else if (key == "framerate_num") g_config.gst1_framerate_num = std::stoi(value);
                else if (key == "framerate_den") g_config.gst1_framerate_den = std::stoi(value); else if (key == "is_h264_native_source") g_config.gst1_is_h264_native_source = (toLower(value) == "true");
                else if (key == "rtp_payload_type") g_config.gst1_rtp_payload_type = std::stoi(value); else if (key == "rtp_config_interval") g_config.gst1_rtp_config_interval = std::stoi(value);
            } else if (current_section == "gstreamer_camera_2") {
                if (key == "port") g_config.gst2_port = std::stoi(value);
                else if (key == "width") g_config.gst2_width = std::stoi(value);
                else if (key == "height") g_config.gst2_height = std::stoi(value); else if (key == "framerate_num") g_config.gst2_framerate_num = std::stoi(value);
                else if (key == "framerate_den") g_config.gst2_framerate_den = std::stoi(value); else if (key == "is_h264_native_source") g_config.gst2_is_h264_native_source = (toLower(value) == "true");
                else if (key == "rtp_payload_type") g_config.gst2_rtp_payload_type = std::stoi(value); else if (key == "rtp_config_interval") g_config.gst2_rtp_config_interval = std::stoi(value);
                else if (key == "x264_bitrate") g_config.gst2_x264_bitrate = std::stoi(value); else if (key == "x264_tune") g_config.gst2_x264_tune = value;
                else if (key == "x264_speed_preset") g_config.gst2_x264_speed_preset = value;
            } else if (current_section == "config_sync") {
                if (key == "cpp_recv_port") g_config.config_sync_cpp_recv_port = std::stoi(value);
                else if (key == "wpf_host") g_config.config_sync_wpf_host = value;
                else if (key == "wpf_recv_port") g_config.config_sync_wpf_recv_port = std::stoi(value);
            } else {
                std::cerr << "警告: " << filename << " の " << line_num << " 行目: 不明なセクションまたはキー [" << current_section << "] " << key << "=" << value << std::endl;
            }
        } catch (const std::invalid_argument& e) {
            std::cerr << "エラー: " << filename << " の " << line_num << " 行目: 数値変換エラー (" << key << "=" << value << ") - " << e.what() << std::endl;
        } catch (const std::out_of_range& e) {
            std::cerr << "エラー: " << filename << " の " << line_num << " 行目: 数値が範囲外 (" << key << "=" << value << ") - " << e.what() << std::endl;
        }
    }
    std::cout << "設定ファイル '" << filename << "' を読み込みました。" << std::endl;
    return true;
}

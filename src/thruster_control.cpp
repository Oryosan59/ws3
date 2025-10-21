#include "thruster_control.h"
#include <cmath>     // std::abs のため
#include <algorithm> // std::max, std::min のため
#include <stdio.h>   // printf のため
#include "config.h"  // グローバル設定オブジェクト g_config を使用するため

// 現在のPWM値を保持する静的変数（実際に出力される値）
static float current_pwm_values[NUM_THRUSTERS]; // 初期化は thruster_init で行う

// --- 定数 (config.h から移動) ---
// --- ヘルパー関数 ---

// 線形補正関数
static float map_value(float x, float in_min, float in_max, float out_min, float out_max)
{
    if (in_max == in_min)
    {
        // ゼロ除算を回避
        return out_min;
    }
    // マッピング前に入力値を指定範囲内にクランプ
    x = std::max(in_min, std::min(x, in_max));
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// 線形補間による平滑化関数
static float smooth_interpolate(float current_value, float target_value, float smoothing_factor)
{
    // 線形補間: current + (target - current) * factor
    // factor = 0.0 なら変化なし、factor = 1.0 なら即座に目標値に到達
    return current_value + (target_value - current_value) * smoothing_factor;
}

// PWM値を設定するヘルパー (範囲チェックとデューティサイクル計算を含む)
static void set_thruster_pwm(int channel, int pulse_width_us)
{
    // PWM値が有効な動作範囲内にあることを保証するためにクランプ
    // 注意: クランプの上限として PWM_BOOST_MAX を使用
    int clamped_pwm = std::max(g_config.pwm_min, std::min(pulse_width_us, g_config.pwm_boost_max));

    // デューティサイクルを計算
    float duty_cycle = static_cast<float>(clamped_pwm) / (1000000.0f / g_config.pwm_frequency); // PWM_PERIOD_US の計算をインライン化

    // 指定されたチャンネルのPWMデューティサイクルを設定
    set_pwm_channel_duty_cycle(channel, duty_cycle);

    // デバッグ出力 (オプション)
    // printf("Ch%d: Set PWM = %d (Clamped: %d), Duty = %.4f\n", channel, pulse_width_us, clamped_pwm, duty_cycle); // NOLINT
}

// --- モジュール関数 ---

bool thruster_init()
{
    printf("Enabling PWM\n");
    set_pwm_enable(true); // NOLINT
    printf("Setting PWM frequency to %.1f Hz\n", g_config.pwm_frequency);
    set_pwm_freq_hz(g_config.pwm_frequency); // NOLINT

    
    // すべてのスラスターをニュートラル/最小値に初期化
    for (int i = 0; i < NUM_THRUSTERS; ++i)
    { // NOLINT
        set_thruster_pwm(i, g_config.pwm_min);
        current_pwm_values[i] = static_cast<float>(g_config.pwm_min); // 平滑化用の現在値も初期化
    }
    
    // LEDチャンネルを初期状態 (OFF) に設定
    set_thruster_pwm(g_config.led_pwm_channel, g_config.led_pwm_off);
    // 新しいLED2チャンネルを初期状態 (OFF) に設定
    set_thruster_pwm(g_config.led2_pwm_channel, g_config.led2_pwm_off);
    set_thruster_pwm(g_config.led3_pwm_channel, g_config.led3_pwm_off);
    set_thruster_pwm(g_config.led4_pwm_channel, g_config.led4_pwm_off);
    set_thruster_pwm(g_config.led5_pwm_channel, g_config.led5_pwm_off);
    printf("Thrusters initialized to PWM %d. LED on Ch%d initialized to PWM %d (OFF). LED2 on Ch%d initialized to PWM %d (OFF). LED3 on Ch%d initialized to PWM %d (OFF). LED4 on Ch%d initialized to PWM %d (OFF). LED5 on Ch%d initialized to PWM %d (OFF).\n", 
        g_config.pwm_min, 
        g_config.led_pwm_channel, g_config.led_pwm_off, 
        g_config.led2_pwm_channel, g_config.led2_pwm_off,
        g_config.led3_pwm_channel, g_config.led3_pwm_off,
        g_config.led4_pwm_channel, g_config.led4_pwm_off,
        g_config.led5_pwm_channel, g_config.led5_pwm_off);
    return true;
}

void thruster_disable()
{
    printf("Disabling PWM\n");
    for (int i = 0; i < NUM_THRUSTERS; ++i)
    { // NOLINT
        set_thruster_pwm(i, g_config.pwm_min);
        current_pwm_values[i] = static_cast<float>(g_config.pwm_min); // 平滑化用の現在値もリセット
    }
    // LEDチャンネルをOFFに設定
    set_thruster_pwm(g_config.led_pwm_channel, g_config.led_pwm_off);
    // 新しいLED2チャンネルをOFFに設定
    set_thruster_pwm(g_config.led2_pwm_channel, g_config.led2_pwm_off);
    set_thruster_pwm(g_config.led3_pwm_channel, g_config.led3_pwm_off);
    set_thruster_pwm(g_config.led4_pwm_channel, g_config.led4_pwm_off);
    set_thruster_pwm(g_config.led5_pwm_channel, g_config.led5_pwm_off);
    set_pwm_enable(false); // NOLINT
}

// 水平スラスター制御ロジック (updateThrustersFromSticksの内容を移植・調整)
static void update_horizontal_thrusters(const GamepadData &data, const AxisData &gyro_data, int target_pwm_out[4])
{
    // 目標PWM配列をニュートラル/最小値に初期化
    for (int i = 0; i < 4; ++i) { // NOLINT
        target_pwm_out[i] = g_config.pwm_min;
    }

    bool lx_active = std::abs(data.leftThumbX) > g_config.joystick_deadzone;
    bool rx_active = std::abs(data.rightThumbX) > g_config.joystick_deadzone;

    int pwm_lx[4] = {g_config.pwm_min, g_config.pwm_min, g_config.pwm_min, g_config.pwm_min}; // NOLINT
    int pwm_rx[4] = {g_config.pwm_min, g_config.pwm_min, g_config.pwm_min, g_config.pwm_min}; // NOLINT

    // Lx (回転) の寄与 (PWM_MIN - PWM_NORMAL_MAX にマッピング)
    if (data.leftThumbX < -g_config.joystick_deadzone)
    { // 左旋回
        int val = static_cast<int>(map_value(data.leftThumbX, -32768, -g_config.joystick_deadzone, g_config.pwm_normal_max, g_config.pwm_min));
        pwm_lx[1] = val; // Ch 1 (前右)
        pwm_lx[2] = val; // Ch 2 (後左)
    }
    else if (data.leftThumbX > g_config.joystick_deadzone)
    { // 右旋回
        int val = static_cast<int>(map_value(data.leftThumbX, g_config.joystick_deadzone, 32767, g_config.pwm_min, g_config.pwm_normal_max));
        pwm_lx[0] = val; // Ch 0 (前左)
        pwm_lx[3] = val; // Ch 3 (後右)
    }

    // Rx (平行移動) の寄与 (PWM_MIN - PWM_NORMAL_MAX にマッピング)
    if (data.rightThumbX < -g_config.joystick_deadzone)
    { // 左平行移動
        int val = static_cast<int>(map_value(data.rightThumbX, -32768, -g_config.joystick_deadzone, g_config.pwm_normal_max, g_config.pwm_min));
        pwm_rx[1] = val; // Ch 1 (前右)
        pwm_rx[3] = val; // Ch 3 (後右)
    }
    else if (data.rightThumbX > g_config.joystick_deadzone)
    { // 右平行移動
        int val = static_cast<int>(map_value(data.rightThumbX, g_config.joystick_deadzone, 32767, g_config.pwm_min, g_config.pwm_normal_max));
        pwm_rx[0] = val; // Ch 0 (前左)
        pwm_rx[2] = val; // Ch 2 (後左)
    }

    // 両方のスティックがアクティブな場合、寄与を結合してブーストを適用
    if (lx_active && rx_active)
    {
        const int boost_range = g_config.pwm_boost_max - g_config.pwm_normal_max;
        int abs_lx = std::abs(data.leftThumbX);
        int abs_rx = std::abs(data.rightThumbX);
        int weaker_input_abs = std::min(abs_lx, abs_rx);
        int boost_add = static_cast<int>(map_value(weaker_input_abs, g_config.joystick_deadzone, 32768, 0, boost_range));

        // スティックの方向に基づいてブーストされるチャンネルを決定
        if (data.leftThumbX < 0 && data.rightThumbX < 0)
        { // 左旋回 + 左平行移動 -> Ch 1 (FR) をブースト
            target_pwm_out[0] = std::max(pwm_lx[0], pwm_rx[0]);
            target_pwm_out[1] = std::max(pwm_lx[1], pwm_rx[1]) + boost_add;
            target_pwm_out[2] = std::max(pwm_lx[2], pwm_rx[2]);
            target_pwm_out[3] = std::max(pwm_lx[3], pwm_rx[3]);
        }
        else if (data.leftThumbX < 0 && data.rightThumbX > 0)
        { // 左旋回 + 右平行移動 -> Ch 2 (RL) をブースト
            target_pwm_out[0] = std::max(pwm_lx[0], pwm_rx[0]);
            target_pwm_out[1] = std::max(pwm_lx[1], pwm_rx[1]);
            target_pwm_out[2] = std::max(pwm_lx[2], pwm_rx[2]) + boost_add;
            target_pwm_out[3] = std::max(pwm_lx[3], pwm_rx[3]);
        }
        else if (data.leftThumbX > 0 && data.rightThumbX < 0)
        { // 右旋回 + 左平行移動 -> Ch 3 (RR) をブースト
            target_pwm_out[0] = std::max(pwm_lx[0], pwm_rx[0]);
            target_pwm_out[1] = std::max(pwm_lx[1], pwm_rx[1]);
            target_pwm_out[2] = std::max(pwm_lx[2], pwm_rx[2]);
            target_pwm_out[3] = std::max(pwm_lx[3], pwm_rx[3]) + boost_add;
        }
        else
        { // 右旋回 + 右平行移動 -> Ch 0 (FL) をブースト
            target_pwm_out[0] = std::max(pwm_lx[0], pwm_rx[0]) + boost_add;
            target_pwm_out[1] = std::max(pwm_lx[1], pwm_rx[1]);
            target_pwm_out[2] = std::max(pwm_lx[2], pwm_rx[2]);
            target_pwm_out[3] = std::max(pwm_lx[3], pwm_rx[3]);
        }
    }
    else
    {
        // 一方のスティックのみアクティブ、またはどちらも非アクティブ: 単純な組み合わせ (最大値)
        for (int i = 0; i < 4; ++i)
        {
            target_pwm_out[i] = std::max(pwm_lx[i], pwm_rx[i]);
        }
    }

    // --- ジャイロによるロール安定化補正 (エルロン操作時) ---
    if (rx_active) // エルロン操作中のみ安定化制御を行う
    {
        // --- ロール補正 ---
        float roll_rate = gyro_data.x; // X軸はロールレート
        const float Kp_roll = g_config.kp_roll;
        int correction_pwm_roll = static_cast<int>(roll_rate * Kp_roll);

        target_pwm_out[0] -= correction_pwm_roll;
        target_pwm_out[1] += correction_pwm_roll;
        target_pwm_out[2] += correction_pwm_roll;
        target_pwm_out[3] -= correction_pwm_roll;

        // --- ヨー補正 (Z軸回転の調整) ---
        float yaw_rate = gyro_data.z; // Z軸はヨーレート
        const float Kp_yaw = g_config.kp_yaw;
        int correction_pwm_yaw = static_cast<int>(yaw_rate * Kp_yaw);

        target_pwm_out[0] -= correction_pwm_yaw;
        target_pwm_out[1] += correction_pwm_yaw;
        target_pwm_out[2] += correction_pwm_yaw;
        target_pwm_out[3] -= correction_pwm_yaw;
    }

    // --- GyroによるYaw補正 (Rx入力時にZ軸回転しないよう補正) ---
    if (!lx_active)
    {
        const float yaw_threshold_dps = g_config.yaw_threshold_dps;
        const float yaw_gain = g_config.yaw_gain;
        float yaw_rate = -gyro_data.z;

        if (std::abs(yaw_rate) > yaw_threshold_dps)
        {
            int yaw_pwm = static_cast<int>(yaw_rate * -yaw_gain);
            yaw_pwm = std::max(-400, std::min(400, yaw_pwm)); // 補正の最大値をクランプ

            if (yaw_pwm < 0)
            {
                target_pwm_out[0] = std::min(g_config.pwm_boost_max, target_pwm_out[0] + std::abs(yaw_pwm));
                target_pwm_out[3] = std::min(g_config.pwm_boost_max, target_pwm_out[3] + std::abs(yaw_pwm));
            }
            else
            {
                target_pwm_out[1] = std::min(g_config.pwm_boost_max, target_pwm_out[1] + yaw_pwm);
                target_pwm_out[2] = std::min(g_config.pwm_boost_max, target_pwm_out[2] + yaw_pwm);
            }
        }
    }
}

// 前進/後退スラスター制御ロジック
static int calculate_forward_reverse_pwm(int value)
{
    int pulse_width;
    const int current_max_pwm = g_config.pwm_boost_max;
    const int current_min_pwm = g_config.pwm_min;

    if (value <= g_config.joystick_deadzone)
    {
        pulse_width = g_config.pwm_min;
    }
    else
    {
        pulse_width = static_cast<int>(map_value(value, g_config.joystick_deadzone, 32767, g_config.pwm_min, current_max_pwm));
    }
    return pulse_width;
}

// メインの更新関数（平滑化機能付き）
void thruster_update(const GamepadData &gamepad_data, const AxisData &gyro_data)
{
    // --- 目標PWM値の計算 ---
    int target_horizontal_pwm[4];
    update_horizontal_thrusters(gamepad_data, gyro_data, target_horizontal_pwm);

    // 前進/後退の目標PWM値
    int target_forward_pwm = calculate_forward_reverse_pwm(gamepad_data.rightThumbY);

    // --- 平滑化処理：現在値を目標値に向けて線形補間 ---
    
    // 水平スラスター (Ch0-3) の平滑化
    for (int i = 0; i < 4; ++i)
    {
        current_pwm_values[i] = smooth_interpolate(
            current_pwm_values[i], 
            static_cast<float>(target_horizontal_pwm[i]),
            g_config.smoothing_factor_horizontal
        );
    }
    
    // 前進/後退スラスター (Ch4, Ch5) の平滑化
    float target_forward_val = static_cast<float>(target_forward_pwm);

    if (target_forward_val > current_pwm_values[4]) { // 加速時のみ平滑化
        current_pwm_values[4] = smooth_interpolate(
            current_pwm_values[4], 
            target_forward_val,
            g_config.smoothing_factor_vertical
        );
    } else { // 減速時または維持時は即座に適用
        current_pwm_values[4] = target_forward_val;
    }

    if (target_forward_val > current_pwm_values[5]) { // 加速時のみ平滑化
        current_pwm_values[5] = smooth_interpolate(
            current_pwm_values[5], 
            target_forward_val,
            g_config.smoothing_factor_vertical
        );
    } else { // 減速時または維持時は即座に適用
        current_pwm_values[5] = target_forward_val;
    }

    // --- PWM信号をスラスターに送信 ---
    printf("--- Thruster and LED PWM (Smoothed) ---\n");
    
    // 水平スラスター
    for (int i = 0; i < 4; ++i)
    {
        int smoothed_pwm = static_cast<int>(current_pwm_values[i]);
        set_thruster_pwm(i, smoothed_pwm);
        printf("Ch%d: Target=%d, Smoothed=%d\n", i, target_horizontal_pwm[i], smoothed_pwm);
    }
    
    // 前進/後退スラスター
    int smoothed_forward_pwm = static_cast<int>(current_pwm_values[4]);
    set_thruster_pwm(4, smoothed_forward_pwm);
    set_thruster_pwm(5, smoothed_forward_pwm);
    printf("Ch4&5: Target=%d, Smoothed=%d\n", target_forward_pwm, smoothed_forward_pwm);

    // --- LED制御 (平滑化なし) ---
    static int current_led_pwm = g_config.led_pwm_off;
    static bool y_button_previously_pressed = false;

    bool y_button_currently_pressed = (gamepad_data.buttons & GamepadButton::Y);

    if (y_button_currently_pressed && !y_button_previously_pressed)
    {
        if (current_led_pwm == g_config.led_pwm_off)
        {
            current_led_pwm = g_config.led_pwm_on;
        }
        else
        {
            current_led_pwm = g_config.led_pwm_off;
        }
    }
    y_button_previously_pressed = y_button_currently_pressed;

    set_thruster_pwm(g_config.led_pwm_channel, current_led_pwm);
    printf("Ch%d: LED PWM = %d (%s)\n", g_config.led_pwm_channel, current_led_pwm, (current_led_pwm == g_config.led_pwm_on ? "ON" : "OFF"));

    // --- LED 2 (十字キー上) 制御 ---
    static int current_led2_pwm = g_config.led2_pwm_off;
    static bool dpad_up_button_previously_pressed = false;

    bool dpad_up_button_currently_pressed = (gamepad_data.buttons & GamepadButton::DPadUp);

    if (dpad_up_button_currently_pressed && !dpad_up_button_previously_pressed)
    {
        if (current_led2_pwm == g_config.led2_pwm_off) {
            current_led2_pwm = g_config.led2_pwm_on1;
        } else if (current_led2_pwm == g_config.led2_pwm_on1) {
            current_led2_pwm = g_config.led2_pwm_on2;
        } else if (current_led2_pwm == g_config.led2_pwm_on2) {
            current_led2_pwm = g_config.led2_pwm_max;
        } else {
            current_led2_pwm = g_config.led2_pwm_off;
        }
    }
    dpad_up_button_previously_pressed = dpad_up_button_currently_pressed;

    set_thruster_pwm(g_config.led2_pwm_channel, current_led2_pwm);
    const char* led2_state_str = "UNKNOWN";
    if (current_led2_pwm == g_config.led2_pwm_off) led2_state_str = "OFF";
    else if (current_led2_pwm == g_config.led2_pwm_on1) led2_state_str = "ON1";
    else if (current_led2_pwm == g_config.led2_pwm_on2) led2_state_str = "ON2";
    else if (current_led2_pwm == g_config.led2_pwm_max) led2_state_str = "MAX";
    printf("Ch%d: LED2 PWM = %d (%s)\n", g_config.led2_pwm_channel, current_led2_pwm, led2_state_str);

    // --- LED 3 (十字キー下) 制御 ---
    static int current_led3_pwm = g_config.led3_pwm_off;
    static bool dpad_down_button_previously_pressed = false;

    bool dpad_down_button_currently_pressed = (gamepad_data.buttons & GamepadButton::DPadDown);

    if (dpad_down_button_currently_pressed && !dpad_down_button_previously_pressed)
    {
        if (current_led3_pwm == g_config.led3_pwm_off) {
            current_led3_pwm = g_config.led3_pwm_on1;
        } else if (current_led3_pwm == g_config.led3_pwm_on1) {
            current_led3_pwm = g_config.led3_pwm_on2;
        } else if (current_led3_pwm == g_config.led3_pwm_on2) {
            current_led3_pwm = g_config.led3_pwm_max;
        } else {
            current_led3_pwm = g_config.led3_pwm_off;
        }
    }
    dpad_down_button_previously_pressed = dpad_down_button_currently_pressed;

    set_thruster_pwm(g_config.led3_pwm_channel, current_led3_pwm);
    const char* led3_state_str = "UNKNOWN";
    if (current_led3_pwm == g_config.led3_pwm_off) led3_state_str = "OFF";
    else if (current_led3_pwm == g_config.led3_pwm_on1) led3_state_str = "ON1";
    else if (current_led3_pwm == g_config.led3_pwm_on2) led3_state_str = "ON2";
    else if (current_led3_pwm == g_config.led3_pwm_max) led3_state_str = "MAX";
    printf("Ch%d: LED3 PWM = %d (%s)\n", g_config.led3_pwm_channel, current_led3_pwm, led3_state_str);

    // --- LED 4 (十字キー左) 制御 ---
    static int current_led4_pwm = g_config.led4_pwm_off;
    static bool dpad_left_button_previously_pressed = false;

    bool dpad_left_button_currently_pressed = (gamepad_data.buttons & GamepadButton::DPadLeft);

    if (dpad_left_button_currently_pressed && !dpad_left_button_previously_pressed)
    {
        if (current_led4_pwm == g_config.led4_pwm_off) {
            current_led4_pwm = g_config.led4_pwm_on1;
        } else if (current_led4_pwm == g_config.led4_pwm_on1) {
            current_led4_pwm = g_config.led4_pwm_on2;
        } else if (current_led4_pwm == g_config.led4_pwm_on2) {
            current_led4_pwm = g_config.led4_pwm_max;
        } else {
            current_led4_pwm = g_config.led4_pwm_off;
        }
    }
    dpad_left_button_previously_pressed = dpad_left_button_currently_pressed;

    set_thruster_pwm(g_config.led4_pwm_channel, current_led4_pwm);
    const char* led4_state_str = "UNKNOWN";
    if (current_led4_pwm == g_config.led4_pwm_off) led4_state_str = "OFF";
    else if (current_led4_pwm == g_config.led4_pwm_on1) led4_state_str = "ON1";
    else if (current_led4_pwm == g_config.led4_pwm_on2) led4_state_str = "ON2";
    else if (current_led4_pwm == g_config.led4_pwm_max) led4_state_str = "MAX";
    printf("Ch%d: LED4 PWM = %d (%s)\n", g_config.led4_pwm_channel, current_led4_pwm, led4_state_str);

    // --- LED 5 (十字キー右) 制御 ---
    static int current_led5_pwm = g_config.led5_pwm_off;
    static bool dpad_right_button_previously_pressed = false;

    bool dpad_right_button_currently_pressed = (gamepad_data.buttons & GamepadButton::DPadRight);

    if (dpad_right_button_currently_pressed && !dpad_right_button_previously_pressed)
    {
        if (current_led5_pwm == g_config.led5_pwm_off) {
            current_led5_pwm = g_config.led5_pwm_on1;
        } else if (current_led5_pwm == g_config.led5_pwm_on1) {
            current_led5_pwm = g_config.led5_pwm_on2;
        } else if (current_led5_pwm == g_config.led5_pwm_on2) {
            current_led5_pwm = g_config.led5_pwm_max;
        } else {
            current_led5_pwm = g_config.led5_pwm_off;
        }
    }
    dpad_right_button_previously_pressed = dpad_right_button_currently_pressed;

    set_thruster_pwm(g_config.led5_pwm_channel, current_led5_pwm);
    const char* led5_state_str = "UNKNOWN";
    if (current_led5_pwm == g_config.led5_pwm_off) led5_state_str = "OFF";
    else if (current_led5_pwm == g_config.led5_pwm_on1) led5_state_str = "ON1";
    else if (current_led5_pwm == g_config.led5_pwm_on2) led5_state_str = "ON2";
    else if (current_led5_pwm == g_config.led5_pwm_max) led5_state_str = "MAX";
    printf("Ch%d: LED5 PWM = %d (%s)\n", g_config.led5_pwm_channel, current_led5_pwm, led5_state_str);




    printf("--------------------\n");
}

// すべてのスラスターを指定されたPWM値に設定し、LEDをオフにする関数
void thruster_set_all_pwm(int pwm_value)
{
    for (int i = 0; i < NUM_THRUSTERS; ++i)
    {
        set_thruster_pwm(i, pwm_value);
        current_pwm_values[i] = static_cast<float>(pwm_value); // 平滑化用の現在値も更新
    }
    set_thruster_pwm(g_config.led_pwm_channel, g_config.led_pwm_off);
    set_thruster_pwm(g_config.led2_pwm_channel, g_config.led2_pwm_off);
    set_thruster_pwm(g_config.led3_pwm_channel, g_config.led3_pwm_off);
    set_thruster_pwm(g_config.led4_pwm_channel, g_config.led4_pwm_off);
    set_thruster_pwm(g_config.led5_pwm_channel, g_config.led5_pwm_off);
}

// 平滑化係数を動的に変更する関数（オプション）

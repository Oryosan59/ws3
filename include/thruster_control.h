#ifndef THRUSTER_CONTROL_H // インクルードガード: ヘッダーファイルが複数回インクルードされるのを防ぐ
#define THRUSTER_CONTROL_H // インクルードガード

#include "gamepad.h"   // GamepadData 構造体の定義が必要なためインクルード
#include "bindings.h"  // AxisData 構造体を使用するため (read_gyro() の戻り値型)
#include "config.h"    // グローバル設定オブジェクト g_config を使用するため

// --- 定数定義 ---
// NUM_THRUSTERS はハードウェア固定値なので、ここでは定数として残す

#define NUM_THRUSTERS 6        // 制御対象のスラスター総数 (Ch0-3 水平, Ch4-5 前進/後退)

// --- LED制御用定数 ---
// LED_PWM_CHANNEL, LED_PWM_ON, LED_PWM_OFF は config.h/cpp に移動

// --- 関数のプロトタイプ宣言 ---
// スラスター制御モジュールを初期化する (PWM設定など)
bool thruster_init();
// スラスター制御を無効化する (PWM停止など)
void thruster_disable();
// ゲームパッドデータとジャイロデータに基づいてすべてのスラスターのPWM出力を更新する
void thruster_update(const GamepadData &gamepad_data, const AxisData &gyro_data);
// 全てのスラスターを指定されたPWM値に設定し、LEDをオフにする (フェイルセーフ用)
void thruster_set_all_pwm(int pwm_value);
// ヘルパー関数（他の場所で必要ない場合は .cpp 内部に保持できます）
// float map_value(float x, float in_min, float in_max, float out_min, float out_max);

#endif // THRUSTER_CONTROL_H

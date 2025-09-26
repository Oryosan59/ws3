#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <stdint.h> // 固定幅整数型 (uint16_t など) を使用するため
#include <string>   // std::string を使用するため
#include <vector>   // 将来的な使用や代替のパース方法のために含める (現在は未使用)

// ゲームパッドからの受信データを格納する構造体
struct GamepadData
{
    int leftThumbX = 0;   // 左スティック X軸 (-32768 ~ 32767)
    int leftThumbY = 0;   // 左スティック Y軸 (-32768 ~ 32767)
    int rightThumbX = 0;  // 右スティック X軸 (-32768 ~ 32767)
    int rightThumbY = 0;  // 右スティック Y軸 (-32768 ~ 32767)
    int LT = 0;           // 左トリガー (0 ~ 1023?)
    int RT = 0;           // 右トリガー (0 ~ 1023?)
    uint16_t buttons = 0; // ボタンの状態 (ビットフラグ)
};

// ゲームパッドのボタンを表すビットフラグの定義
enum GamepadButton : uint32_t // 基底型を明示的に指定
{
    None = 0x0000,      // ボタンが押されていない状態
    DPadUp = 0x0001,    // 十字キー 上
    DPadDown = 0x0002,  // 十字キー 下
    DPadLeft = 0x0004,  // 十字キー 左
    DPadRight = 0x0008, // 十字キー 右
    Start = 0x0010,         // Start ボタン (標準的な値 0x0010)
    Back = 0x0020,          // Back ボタン (標準的な値 0x0020)
    LeftShoulder = 0x0100,  // Lボタン (LB) (標準的な値 0x0100)
    RightShoulder = 0x0200, // Rボタン (RB) (標準的な値 0x0200)
    A = 0x1000,             // A ボタン (標準的な値 0x1000)
    B = 0x2000,             // B ボタン (標準的な値 0x2000)
    X = 0x4000,             // X ボタン (標準的な値 0x4000)
    Y = 0x8000              // Y ボタン (標準的な値 0x8000)
};

// 関数のプロトタイプ宣言
// 受信した文字列データを GamepadData 構造体にパースする関数
GamepadData parseGamepadData(const std::string &data);

#endif // GAMEPAD_H

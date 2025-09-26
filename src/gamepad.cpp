#include "gamepad.h" // GamepadData 構造体と GamepadButton 列挙型の定義
#include <sstream>   // 文字列ストリーム (std::stringstream) を使用するため
#include <iostream>  // 標準入出力 (std::cerr) を使用するため
#include <stdexcept> // 例外クラス (std::invalid_argument, std::out_of_range) を使用するため

// ヘルパー関数: 文字列の前後の空白文字 (スペース、タブ、改行など) を削除する
std::string trim(const std::string &str)
{
    size_t first = str.find_first_not_of(" \t\n\r\f\v");
    if (std::string::npos == first)
    {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(first, (last - first + 1));
}

// 受信した文字列データを GamepadData 構造体にパースする関数
GamepadData parseGamepadData(const std::string &data)
{
    GamepadData gamepad;           // パース結果を格納する構造体 (デフォルト値で初期化)
    std::stringstream ss(data);    // 受信した文字列データから文字列ストリームを作成
    std::string token;             // 分割された各トークン (部分文字列) を格納する変数
    int values[7] = {0};           // パースされた整数値を一時的に格納する配列 (7つの要素: LX, LY, RX, RY, LT, RT, Buttons)
    int index = 0;                 // values 配列の現在のインデックス
    const int EXPECTED_VALUES = 7; // 期待される値の総数

    // 文字列ストリームからカンマ (',') 区切りでトークンを読み込むループ
    while (std::getline(ss, token, ',') && index < EXPECTED_VALUES)
    {
        try
        {
            std::string trimmed_token = trim(token); // トークンの前後の空白を削除
            if (!trimmed_token.empty())
            {
                // 空でないトークンは整数に変換して values 配列に格納
                values[index++] = std::stoi(trimmed_token); // string to integer
            }
            else
            {
                // 空のトークンを処理 - 0 として扱う
                values[index++] = 0;
                std::cerr << "警告: 空のトークンを検出。0として扱います。" << std::endl;
            }
        }
        catch (const std::invalid_argument &e) // stoi が変換できない形式の場合
        {
            std::cerr << "stoiエラー: 無効なデータ形式 (" << token << ") - " << e.what() << std::endl;
            // 致命的なパースエラーが発生した場合、デフォルトのゲームパッドデータを返す
            return GamepadData{}; // デフォルト初期化された構造体を返す
        }
        catch (const std::out_of_range &e) // stoi の結果が int の範囲外の場合
        {
            std::cerr << "stoiエラー: 数値が範囲外 (" << token << ") - " << e.what() << std::endl;
            // 致命的なパースエラーが発生した場合、デフォルトのゲームパッドデータを返す
            return GamepadData{}; // デフォルト初期化された構造体を返す
        }
    }

    // 期待される数の値を受信したかチェック
    if (index < EXPECTED_VALUES)
    {
        std::cerr << "警告: 受信データが不足しています。項目数: " << index << " (期待値: " << EXPECTED_VALUES << ")" << std::endl;
        // 要件によっては、ここでデフォルトデータを返すことも検討できる
    }

    // 解析した値を構造体のメンバーに割り当てる
    gamepad.leftThumbX = values[0];
    gamepad.leftThumbY = values[1];
    gamepad.rightThumbX = values[2];
    gamepad.rightThumbY = values[3];
    gamepad.LT = values[4];
    gamepad.RT = values[5];
    // ボタンの値を安全に uint16_t にキャスト
    gamepad.buttons = static_cast<uint16_t>(values[6]);

    return gamepad; // パースされたデータを返す
}

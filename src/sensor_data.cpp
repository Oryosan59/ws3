// --- インクルード ---
#include "sensor_data.h" // このモジュールのヘッダーファイル
#include "bindings.h"    // ハードウェア読み取り関数 (read_*) を使用するため
#include <stdio.h>       // 標準入出力関数 (snprintf) を使用するため
#include <iostream>      // 標準エラー出力 (std::cerr) を使用するため

// 関連するすべてのセンサーを読み取り、指定されたバッファに文字列としてフォーマットする関数
bool read_and_format_sensor_data(char *buffer, size_t buffer_size)
{
    // 引数チェック: バッファポインタが NULL またはバッファサイズが 0 の場合は失敗
    if (!buffer || buffer_size == 0)
    {
        return false;
    }

    // --- センサーデータの取得 ---
    float temperature = read_temp();  // 温度センサーの値を読み取る
    float pressure = read_pressure(); // 圧力センサーの値を読み取る
    bool leak = read_leak();          // リークセンサーの状態を読み取る (true: 漏れあり, false: 漏れなし)
    float adc[4];                     // ADC (アナログ-デジタル変換器) の値を格納する配列
    read_adc_all(adc, 4);             // すべてのADCチャンネルの値を読み取る (read_adc_all が効率的であると仮定)
    AxisData accel = read_accel();    // 加速度センサーの値を読み取る (X, Y, Z軸)
    AxisData gyro = read_gyro();      // ジャイロセンサーの値を読み取る (X, Y, Z軸)
    AxisData mag = read_mag();        // 磁力センサーの値を読み取る (X, Y, Z軸)

    // --- 文字列へのフォーマット ---
    // snprintf を使用して、取得したセンサーデータをカンマ区切りの文字列にフォーマットする
    // 各センサー値にラベルを付け、固定小数点数 (%.6f) または整数 (%d) で表現する
    int written = snprintf(buffer, buffer_size, // NOLINT
                           "TEMP:%.6f,PRESSURE:%.6f,LEAK:%d,"
                           "ADC0:%.6f,ADC1:%.6f,ADC2:%.6f,ADC3:%.6f,"
                           "ACCX:%.6f,ACCY:%.6f,ACCZ:%.6f,"
                           "GYROX:%.6f,GYROY:%.6f,GYROZ:%.6f,"
                           "MAGX:%.6f,MAGY:%.6f,MAGZ:%.6f",
                           temperature, pressure, leak ? 1 : 0,
                           adc[0], adc[1], adc[2], adc[3],
                           accel.x, accel.y, accel.z,
                           gyro.x, gyro.y, gyro.z,
                           mag.x, mag.y, mag.z);

    // --- エラーチェック ---
    // snprintf の戻り値を確認
    if (written < 0)
    {
        // snprintf がエラーを返した場合 (負の値)
        std::cerr << "エラー: センサーデータ文字列のフォーマットに失敗しました (snprintf)。" << std::endl;
        buffer[0] = '\0'; // エラー時にはバッファを空文字列にする
        return false;
    }
    else if ((size_t)written >= buffer_size)
    {
        // 書き込まれた文字数 (written) がバッファサイズ以上の場合、データが切り捨てられたことを意味する
        std::cerr << "警告: センサーデータ文字列がバッファサイズを超えました。切り捨てられました。" << std::endl;
        // データは切り捨てられたが、受信側によってはまだ利用可能かもしれない
        // 切り捨てが致命的な場合は false を返すか、より深刻なログを記録することを検討する
        return true; // 現在は切り捨てられても成功として扱う (要件に応じて false に変更)
    }

    return true; // フォーマット成功
}

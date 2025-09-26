#ifndef SENSOR_DATA_H // インクルードガード: ヘッダーファイルが複数回インクルードされるのを防ぐ
#define SENSOR_DATA_H // インクルードガード

#include <string>   // std::string を使用するため (現在は直接使用していないが、将来的に使う可能性あり)
#include <vector>   // ADCデータなどの配列データを扱うために含める (現在は直接使用していない)
#include <stddef.h> // size_t 型を使用するため

#define SENSOR_BUFFER_SIZE 512 // センサーデータを格納する文字列バッファの推奨サイズ

// 関数のプロトタイプ宣言
// 関連するすべてのセンサーを読み取り、指定されたバッファに文字列としてフォーマットする
// 成功した場合は true、失敗した場合は false を返す。出力文字列は buffer に格納される。
bool read_and_format_sensor_data(char *buffer, size_t buffer_size); // バッファとそのサイズを引数にとる

#endif // SENSOR_DATA_H // インクルードガード終了

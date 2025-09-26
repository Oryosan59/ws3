#ifndef NETWORK_H
#define NETWORK_H

#include <netinet/in.h> // sockaddr_in 構造体やインターネット関連関数を使用するため
#include <sys/time.h>   // struct timeval を使用するため
#include <stdbool.h>    // bool 型を使用するため
#include <stddef.h>     // size_t 型を使用するため

#define DEFAULT_RECV_PORT 12345 // デフォルトの受信UDPポート番号
#define DEFAULT_SEND_PORT 12346 // デフォルトの送信UDPポート番号
#define NET_BUFFER_SIZE 1024    // ネットワーク送受信バッファのサイズ (バイト単位)

// ネットワーク通信の状態を保持する構造体
typedef struct
{
    int recv_socket;                     // 受信に使用するソケットファイルディスクリプタ
    int send_socket;                     // 送信に使用するソケットファイルディスクリプタ
    struct sockaddr_in server_addr;      // このプログラム（サーバー）のアドレス情報
    struct sockaddr_in client_addr_recv; // 最後にデータを受信したクライアントのアドレス情報
    struct sockaddr_in client_addr_send; // データの送信先となるクライアントのアドレス情報
    socklen_t client_addr_len;           // client_addr_recv のサイズを格納する変数
    bool client_addr_known;              // 送信先クライアントアドレスが設定されているかを示すフラグ
    struct timeval last_successful_recv_time; // 最後にデータパケットを正常に受信した時刻
} NetworkContext;

// 関数のプロトタイプ宣言
bool network_init(NetworkContext *ctx);                                         // ネットワークコンテキストを初期化し、ソケットを作成・バインドする
void network_close(NetworkContext *ctx);                                        // ネットワーク関連のリソース（ソケット）を解放する
ssize_t network_receive(NetworkContext *ctx, char *buffer, size_t buffer_size); // UDPデータを受信する (ノンブロッキング)
bool network_send(NetworkContext *ctx, const char *data, size_t data_len);      // UDPデータを送信する
bool network_update_send_address(NetworkContext *ctx);                          // 最後に受信したクライアントのアドレスを送信先として設定するヘルパー関数

#endif // NETWORK_H

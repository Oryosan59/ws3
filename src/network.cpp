#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include "config.h" // g_config を使用するため
#include <sys/time.h> // gettimeofday のため

// ネットワーク送受信コンテキストを初期化する関数
bool network_init(NetworkContext *ctx)
{
    if (!ctx)
        return false; // コンテキストポインタが無効なら失敗

    // 設定オブジェクトからポート番号を取得
    int recv_port = g_config.network_recv_port;
    int send_port = g_config.network_send_port;

    // コンテキスト初期化
    memset(ctx, 0, sizeof(NetworkContext));
    ctx->recv_socket = -1;
    ctx->send_socket = -1;
    ctx->client_addr_len = sizeof(ctx->client_addr_recv);
    ctx->client_addr_known = false;
    gettimeofday(&ctx->last_successful_recv_time, NULL); // 現在時刻で初期化

    // --- 受信ソケット設定 ---
    ctx->recv_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->recv_socket < 0)
    {
        perror("受信ソケット作成失敗");
        return false;
    }

    // ノンブロッキング設定
    int flags = fcntl(ctx->recv_socket, F_GETFL, 0);
    if (flags == -1 || fcntl(ctx->recv_socket, F_SETFL, flags | O_NONBLOCK) == -1) // 現在のフラグを取得し、O_NONBLOCK を追加
    {
        perror("受信ソケットのノンブロッキング設定失敗");
        close(ctx->recv_socket);
        ctx->recv_socket = -1;
        return false;
    }

    // サーバー（このプログラム）のアドレス情報を設定
    memset(&ctx->server_addr, 0, sizeof(ctx->server_addr));
    ctx->server_addr.sin_family = AF_INET;
    ctx->server_addr.sin_addr.s_addr = INADDR_ANY;
    ctx->server_addr.sin_port = htons(recv_port);

    // ソケットにアドレス情報を割り当て (バインド)
    if (bind(ctx->recv_socket, (const struct sockaddr *)&ctx->server_addr, sizeof(ctx->server_addr)) < 0)
    {
        perror("受信ソケットのバインド失敗");
        close(ctx->recv_socket);
        ctx->recv_socket = -1;
        return false;
    }
    printf("UDPサーバー起動 (受信ポート: %d)\n", recv_port);

    // --- 送信ソケット設定 ---
    ctx->send_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->send_socket < 0)
    {
        perror("送信ソケット作成失敗");
        close(ctx->recv_socket); // 受信ソケットも閉じる
        ctx->recv_socket = -1;
        return false;
    }

    // 送信先アドレスの初期設定 (ポートのみ)
    memset(&ctx->client_addr_send, 0, sizeof(ctx->client_addr_send));
    ctx->client_addr_send.sin_family = AF_INET;
    ctx->client_addr_send.sin_port = htons(send_port); // 送信ポート番号を設定 (ネットワークバイトオーダーに変換)
    // 送信先IPアドレスは最初の受信時に設定される

    printf("UDP送信準備完了 (送信先ポート: %d)\n", send_port);
    return true;
}

// ネットワーク関連のリソースを解放する関数
void network_close(NetworkContext *ctx)
{
    if (ctx)
    {
        if (ctx->recv_socket >= 0)
        {
            close(ctx->recv_socket);
            ctx->recv_socket = -1;
        }
        if (ctx->send_socket >= 0)
        {
            close(ctx->send_socket);
            ctx->send_socket = -1;
        }
        printf("ソケットをクローズしました。\n");
    }
}

// UDPデータを受信する関数 (ノンブロッキング)
ssize_t network_receive(NetworkContext *ctx, char *buffer, size_t buffer_size)
{
    if (!ctx || ctx->recv_socket < 0 || !buffer || buffer_size == 0)
    {
        return -1; // 引数が無効ならエラー
    }

    // 受信する前にアドレス長をリセット
    ctx->client_addr_len = sizeof(ctx->client_addr_recv);
    ssize_t recv_len = recvfrom(ctx->recv_socket, buffer, buffer_size - 1, 0,
                                (struct sockaddr *)&ctx->client_addr_recv, &ctx->client_addr_len);

    if (recv_len > 0)
    {
        buffer[recv_len] = '\0'; // Null終端

        // --- セキュリティチェック: 許可されたIPアドレスからのパケットか検証 ---
        char client_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ctx->client_addr_recv.sin_addr, client_ip_str, INET_ADDRSTRLEN);
        
        // 設定で許可されたIPアドレス、または "0.0.0.0" (任意) の場合のみ処理を続行
        if (g_config.client_host != "0.0.0.0" &&
            g_config.client_host != client_ip_str)
        {
            fprintf(stderr, "警告: 許可されていないIPアドレス (%s) からのパケットを破棄しました。\n", client_ip_str);
            // パケットを破棄し、受信しなかったのと同じように振る舞う (受信長0を返す)
            // これにより、mainループはタイムアウト機構を正しく動作させることができる
            return 0;
        }

        gettimeofday(&ctx->last_successful_recv_time, NULL); // 最終受信時刻を更新
        // 新しいクライアントか、IPが変わったかチェック
        if (!ctx->client_addr_known || ctx->client_addr_send.sin_addr.s_addr != ctx->client_addr_recv.sin_addr.s_addr)
        {
            network_update_send_address(ctx);
        }
    }
    else if (recv_len < 0)
    {
        // EAGAIN/EWOULDBLOCK はデータがないだけなのでエラーではない
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            perror("受信エラー");
        }
        // エラーまたはデータなしの場合は -1 または 0 を返す recvfrom の仕様に合わせる
    }
    // recv_len が 0 の場合はそのまま 0 を返す (UDPでは通常起こらない)

    return recv_len;
}

// UDPデータを送信する関数
bool network_send(NetworkContext *ctx, const char *data, size_t data_len)
{
    if (!ctx || ctx->send_socket < 0 || !data || !ctx->client_addr_known)
    {
        // 送信先が不明な場合は送信しない
        return false;
    }

    // データ送信試行
    ssize_t sent_len = sendto(ctx->send_socket, data, data_len, 0,
                              (const struct sockaddr *)&ctx->client_addr_send, sizeof(ctx->client_addr_send));

    if (sent_len < 0)
    {
        // クライアント切断時などにログが溢れるのを避けるため、頻繁なエラー出力は避ける
        // perror("送信エラー");
        return false;
    }
    else if ((size_t)sent_len < data_len)
    {
        fprintf(stderr, "警告: データが部分的にしか送信されませんでした。\n");
        return false; // 部分送信もエラー扱いとするか、状況による
    }

    return true;
}

// 最後にデータを受信したクライアントのIPアドレスを送信先として設定/更新する関数
bool network_update_send_address(NetworkContext *ctx)
{
    if (!ctx)
        return false;
    char client_ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ctx->client_addr_recv.sin_addr, client_ip_str, INET_ADDRSTRLEN);
    printf("センサーデータ送信先を設定/更新: %s:%d\n",
           client_ip_str,
           ntohs(ctx->client_addr_send.sin_port));                   // ポートは固定
    ctx->client_addr_send.sin_addr = ctx->client_addr_recv.sin_addr; // IPアドレスを更新
    ctx->client_addr_known = true;
    return true;
}

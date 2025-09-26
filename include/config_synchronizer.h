#ifndef CONFIG_SYNCHRONIZER_H
#define CONFIG_SYNCHRONIZER_H

#include <string>
#include <thread>
#include <atomic>

// 設定が更新され、リロードが必要なことを通知するフラグ。
extern std::atomic<bool> g_config_updated_flag;

class ConfigSynchronizer {
public:
    ConfigSynchronizer(const std::string& config_path);
    ~ConfigSynchronizer();

    void start();
    void stop();

private:
    void run();
    bool load_config();
    void save_config();
    std::string serialize_config();
    void update_config_from_string(const std::string& data);
    bool send_config_to_wpf();
    void handle_client_connection(int client_sock);
    void receive_config_updates();


    std::string m_config_path;
    std::thread m_thread;
    std::atomic<bool> m_shutdown_flag;
};

#endif // CONFIG_SYNCHRONIZER_H

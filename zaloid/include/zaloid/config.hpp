#pragma once
#include <string>

namespace zaloid {

struct Config {
    std::string access_token;
    int poll_interval_sec = 10;
    int page_size = 20;
    std::string log_file = "zaloid.log";

    // OAuth v4 cho OA: app_id + secret_key de tu dong cap/lam moi token
    std::string app_id;
    std::string secret_key;
    std::string tokens_file = "tokens.json";
    int callback_port = 18080;

    // Doc tu file JSON; bien moi truong ZALO_ACCESS_TOKEN uu tien hon file.
    static Config load(const std::string& path);
};

} // namespace zaloid

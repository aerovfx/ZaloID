#include "zaloid/config.hpp"
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace zaloid {

Config Config::load(const std::string& path) {
    Config cfg;
    std::ifstream in(path);
    if (in) {
        nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
        if (!j.is_discarded()) {
            if (j.contains("access_token")) cfg.access_token = j["access_token"].get<std::string>();
            if (j.contains("poll_interval_sec")) cfg.poll_interval_sec = j["poll_interval_sec"].get<int>();
            if (j.contains("page_size")) cfg.page_size = j["page_size"].get<int>();
            if (j.contains("log_file")) cfg.log_file = j["log_file"].get<std::string>();
            if (j.contains("app_id")) cfg.app_id = j["app_id"].get<std::string>();
            if (j.contains("secret_key")) cfg.secret_key = j["secret_key"].get<std::string>();
            if (j.contains("tokens_file")) cfg.tokens_file = j["tokens_file"].get<std::string>();
            if (j.contains("callback_port")) cfg.callback_port = j["callback_port"].get<int>();
        }
    }
    if (const char* env = std::getenv("ZALO_ACCESS_TOKEN"); env && *env) {
        cfg.access_token = env;
    }
    if (const char* env = std::getenv("ZALO_APP_ID"); env && *env) cfg.app_id = env;
    if (const char* env = std::getenv("ZALO_SECRET_KEY"); env && *env) cfg.secret_key = env;
    if (cfg.access_token.empty() && (cfg.app_id.empty() || cfg.secret_key.empty())) {
        throw std::runtime_error(
            "Chua co access_token. Dat bien moi truong ZALO_ACCESS_TOKEN, "
            "truong \"access_token\" trong " + path +
            ", hoac cung cap app_id + secret_key de dang nhap OAuth (lenh \"login\").");
    }
    if (cfg.poll_interval_sec < 1) cfg.poll_interval_sec = 1;
    if (cfg.page_size < 1 || cfg.page_size > 50) cfg.page_size = 20;
    return cfg;
}

} // namespace zaloid

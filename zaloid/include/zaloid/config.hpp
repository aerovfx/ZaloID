#pragma once
#include <string>
#include "zaloid/ocr_client.hpp"

namespace zaloid {

struct Config {
    std::string access_token;
    std::string app_id;
    std::string secret_key;
    std::string tokens_file = "tokens.json";
    int callback_port = 18080;
    int poll_interval_sec = 10;
    int page_size = 20;
    std::string log_file = "zaloid.log";
    OcrConfig ocr;  // OCR configuration
    
    static Config load(const std::string& path);
};

} // namespace zaloid

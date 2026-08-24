#pragma once
#include <atomic>
#include <set>
#include <string>
#include "zaloid/zalo_api.hpp"
#include "zaloid/ocr_client.hpp"
#include "zaloid/config.hpp"
#include "zaloid/http_client.hpp"

namespace zaloid {

class Poller {
public:
    Poller(ZaloApi& api, HttpClient& http, int interval_sec, int page_size, const std::string& log_file, const OcrConfig& ocr_config);
    
    void run(std::atomic<bool>& stop_flag);

private:
    void poll_once();
    void log_line(const std::string& line);
    void process_message_images(const Message& msg);
    std::string format_analysis_log(const Message& msg, const DocumentAnalysis& analysis) const;
    
    ZaloApi& api_;
    HttpClient& http_;
    int interval_sec_;
    int page_size_;
    std::string log_file_;
    std::set<std::string> seen_ids_;
    OcrClient ocr_client_;
};

} // namespace zaloid

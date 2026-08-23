#pragma once
#include <atomic>
#include <set>
#include <string>
#include "zaloid/zalo_api.hpp"

namespace zaloid {

class Poller {
public:
    Poller(ZaloApi& api, int interval_sec, int page_size, const std::string& log_file);

    void run(std::atomic<bool>& stop_flag);

private:
    void poll_once();
    void log_line(const std::string& line);

    ZaloApi& api_;
    int interval_sec_;
    int page_size_;
    std::string log_file_;
    std::set<std::string> seen_ids_;
};

} // namespace zaloid

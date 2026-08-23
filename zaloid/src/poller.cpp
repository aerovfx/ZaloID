#include "zaloid/poller.hpp"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace zaloid {

Poller::Poller(ZaloApi& api, int interval_sec, int page_size, const std::string& log_file)
    : api_(api), interval_sec_(interval_sec), page_size_(page_size), log_file_(log_file) {}

void Poller::log_line(const std::string& line) {
    std::cout << line << std::endl;
    std::ofstream log(log_file_, std::ios::app);
    if (log) log << line << '\n';
    if (!log) std::cerr << "Canh bao: khong ghi duoc log file\n";
    log.close();
}
    
void Poller::poll_once() {
    auto users = api_.list_users(0, page_size_);
    for (const auto& user : users) {
        try {
            auto messages = api_.list_messages(user.id, 0, 10);
            for (const auto& msg : messages) {
                if (msg.id.empty() || seen_ids_.count(msg.id)) continue;
                seen_ids_.insert(msg.id);

                std::ostringstream oss;
                std::time_t t = static_cast<std::time_t>(msg.created_at);
                oss << "[" << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << "] "
                    << (msg.from_name.empty() ? msg.from : msg.from_name)
                    << ": " << msg.text;
                log_line(oss.str());
            }
        } catch (const std::exception& e) {
            log_line(std::string("[loi] khong lay duoc tin cua user ") + user.id + ": " + e.what());
        }
    }
}

void Poller::run(std::atomic<bool>& stop_flag) {
    log_line("Bat dau poll moi " + std::to_string(interval_sec_) + " giay...");
    while (!stop_flag.load()) {
        try {
            poll_once();
        } catch (const std::exception& e) {
            log_line(std::string("[loi] ") + e.what());
        }
        for (int i = 0; i < interval_sec_ && !stop_flag.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    log_line("Da dung.");
}

} // namespace zaloid

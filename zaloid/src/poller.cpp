#include "zaloid/poller.hpp"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace zaloid {

Poller::Poller(ZaloApi& api, HttpClient& http, int interval_sec, int page_size, const std::string& log_file, const OcrConfig& ocr_config)
    : api_(api), http_(http), interval_sec_(interval_sec), page_size_(page_size), log_file_(log_file), ocr_client_(http_, ocr_config) {}

void Poller::log_line(const std::string& line) {
    std::cout << line << std::endl;
    std::ofstream log(log_file_, std::ios::app);
    if (log) log << line << '
';
    if (!log) std::cerr << "Canh bao: khong ghi duoc log file
";
    log.close();
}

void Poller::process_message_images(const Message& msg) {
    if (msg.images.empty()) return;
    
    AnalysisProfile profile = AnalysisProfile::Document;
    std::string profile_str = ocr_client_.get_config().default_profile;
    if (profile_str == "invoice") profile = AnalysisProfile::Invoice;
    else if (profile_str == "report") profile = AnalysisProfile::Report;
    else if (profile_str == "general") profile = AnalysisProfile::General;
    
    for (const auto& img : msg.images) {
        try {
            ImageData img_data;
            img_data.url = img.url;
            img_data.mime_type = img.mime_type;
            img_data.id = img.id.empty() ? "img_" + msg.id : img.id;
            
            log_line("[OCR] Dang phan tich anh: " + img_data.id + " (" + img.url + ")");
            
            DocumentAnalysis analysis = ocr_client_.analyze_image(img_data, profile);
            
            if (analysis.success) {
                std::string log_entry = format_analysis_log(msg, analysis);
                log_line(log_entry);
            } else {
                log_line("[OCR LOI] " + img_data.id + ": " + analysis.error_message);
            }
        } catch (const std::exception& e) {
            log_line("[OCR LOI] Ngoai le: " + std::string(e.what()));
        }
    }
}

std::string Poller::format_analysis_log(const Message& msg, const DocumentAnalysis& analysis) const {
    std::ostringstream oss;
    std::time_t t = static_cast<std::time_t>(msg.created_at);
    oss << "[" << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << "] "
        << (msg.from_name.empty() ? msg.from : msg.from_name)
        << " [OCR PHAN TICH]:";
    
    if (!analysis.summary.empty()) oss << " | Tom tat: " << analysis.summary;
    if (!analysis.document_type.empty()) oss << " | Loai: " << analysis.document_type;
    if (!analysis.reference_number.empty()) oss << " | So ky hieu: " << analysis.reference_number;
    if (!analysis.issue_date.empty()) oss << " | Ngay: " << analysis.issue_date;
    if (!analysis.decision_maker.empty()) oss << " | Nguoi ky: " << analysis.decision_maker;
    if (!analysis.security_level.empty()) oss << " | Do mat: " << analysis.security_level;
    if (!analysis.work_content.empty()) oss << " | Noi dung: " << analysis.work_content;
    if (!analysis.people.empty()) {
        oss << " | Nguoi lien quan: ";
        for (size_t i = 0; i < analysis.people.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << analysis.people[i];
        }
    }
    if (!analysis.keywords.empty()) {
        oss << " | Tu khoa: ";
        for (size_t i = 0; i < analysis.keywords.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << analysis.keywords[i];
        }
    }
    
    return oss.str();
}

void Poller::poll_once() {
    auto users = api_.list_users(0, page_size_);
    for (const auto& user : users) {
        try {
            auto messages = api_.list_messages(user.id, 0, 10);
            for (const auto& msg : messages) {
                if (msg.id.empty() || seen_ids_.count(msg.id)) continue;
                seen_ids_.insert(msg.id);
                
                if (!msg.text.empty()) {
                    std::ostringstream oss;
                    std::time_t t = static_cast<std::time_t>(msg.created_at);
                    oss << "[" << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << "] "
                        << (msg.from_name.empty() ? msg.from : msg.from_name)
                        << ": " << msg.text;
                    log_line(oss.str());
                }
                
                if (ocr_client_.get_config().enabled && !msg.images.empty()) {
                    process_message_images(msg);
                }
            }
        } catch (const std::exception& e) {
            log_line(std::string("[loi] khong lay duoc tin cua user ") + user.id + ": " + e.what());
        }
    }
}

void Poller::run(std::atomic<bool>& stop_flag) {
    log_line("Bat dau poll moi " + std::to_string(interval_sec_) + " giay...");
    if (ocr_client_.get_config().enabled) {
        if (ocr_client_.check_service()) {
            log_line("[OCR] Ket noi Ollama thanh cong (model: " + ocr_client_.get_config().model + ")");
        } else {
            log_line("[OCR CANH BAO] Khong ket noi duoc Ollama tai " + ocr_client_.get_config().endpoint);
        }
    }
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

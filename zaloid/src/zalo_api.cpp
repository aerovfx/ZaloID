#include "zaloid/zalo_api.hpp"
#include <map>
#include <stdexcept>
#include <utility>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace zaloid {

namespace {

const char* kBase = "https://openapi.zalo.me";

std::map<std::string, std::string> auth_headers(const std::string& token) {
    return {{"access_token", token}};
}

std::string url_encode(const std::string& s) {
    char* out = curl_easy_escape(nullptr, s.c_str(), static_cast<int>(s.size()));
    if (!out) throw std::runtime_error("Khong ma hoa duoc URL");
    std::string result = out;
    curl_free(out);
    return result;
}

void check_error(const nlohmann::json& j) {
    if (j.contains("error") && j["error"].is_number_integer() && j["error"].get<int>() != 0) {
        std::string msg = j.value("message", "khong ro");
        throw std::runtime_error("Zalo API error " + std::to_string(j["error"].get<int>()) + ": " + msg);
    }
}

nlohmann::json get_json(HttpClient& http, const std::string& url, const std::string& token) {
    HttpResponse resp = http.get(url, auth_headers(token));
    nlohmann::json j = nlohmann::json::parse(resp.body, nullptr, false);
    if (j.is_discarded()) throw std::runtime_error("Phan hoi khong phai JSON hop le: " + resp.body.substr(0, 200));
    check_error(j);
    return j;
}

} // namespace

ZaloApi::ZaloApi(HttpClient& http, std::string access_token)
    : http_(http), token_(std::move(access_token)) {}

std::vector<User> ZaloApi::list_users(int offset, int count) {
    if (count > 50) count = 50;
    nlohmann::json data = {{"offset", offset}, {"count", count}};
    std::string url = std::string(kBase) + "/v3.0/oa/user/getlist?data=" + url_encode(data.dump());

    nlohmann::json j = get_json(http_, url, token_);

    std::vector<User> out;
    if (!j.contains("data") || !j["data"].contains("users")) return out;
    for (const auto& u : j["data"]["users"]) {
        User user;
        user.id = u.value("user_id", std::string());
        if (!user.id.empty()) out.push_back(std::move(user));
    }
    return out;
}

std::vector<Message> ZaloApi::list_messages(const std::string& user_id, int offset, int count) {
    if (count > 10) count = 10; // API gioi han 10 tin nhan moi request
    nlohmann::json data = {{"user_id", std::stoll(user_id)},
                           {"offset", offset}, {"count", count}};
    std::string url = std::string(kBase) + "/v2.0/oa/conversation?data=" + url_encode(data.dump());

    nlohmann::json j = get_json(http_, url, token_);

    std::vector<Message> out;
    if (!j.contains("data") || !j["data"].is_array()) return out;
    
    for (const auto& m : j["data"]) {
        Message msg;
        msg.id = m.value("message_id", std::string());
        msg.from = m.value("from_id", std::string());
        msg.from_name = m.value("from_display_name", std::string());
        msg.text = m.value("message", std::string());
        msg.created_at = m.value("time", 0LL) / 1000; // ms -> giay
        msg.type = m.value("type", 0);
        
        // Parse image attachments if present
        if (m.contains("attachments") && m["attachments"].is_array()) {
            for (const auto& att : m["attachments"]) {
                ImageAttachment img;
                img.id = att.value("id", std::string());
                img.url = att.value("url", std::string());
                img.mime_type = att.value("mime_type", std::string());
                img.width = att.value("width", 0);
                img.height = att.value("height", 0);
                img.size = att.value("size", 0LL);
                if (!img.url.empty() || !img.id.empty()) {
                    msg.images.push_back(std::move(img));
                }
            }
        }
        
        // Also check for legacy "image" field
        if (msg.images.empty() && m.contains("image") && m["image"].is_object()) {
            ImageAttachment img;
            img.id = m["image"].value("id", std::string());
            img.url = m["image"].value("url", std::string());
            img.mime_type = m["image"].value("mime_type", std::string());
            img.width = m["image"].value("width", 0);
            img.height = m["image"].value("height", 0);
            img.size = m["image"].value("size", 0LL);
            if (!img.url.empty() || !img.id.empty()) {
                msg.images.push_back(std::move(img));
            }
        }
        
        if (!msg.id.empty()) out.push_back(std::move(msg));
    }
    return out;
}

} // namespace zaloid

#pragma once
#include <string>
#include <vector>
#include "zaloid/http_client.hpp"

namespace zaloid {

struct User {
    std::string id; // user_id cua nguoi da tuong tac voi OA
};

struct Message {
    std::string id;         // message_id
    std::string from;       // from_id
    std::string from_name;  // from_display_name
    std::string text;       // noi dung (type "text")
    long long created_at = 0; // unix seconds
};

class ZaloApi {
public:
    explicit ZaloApi(HttpClient& http, std::string access_token);

    // GET /v3.0/oa/user/getlist — danh sach user da tuong tac voi OA
    std::vector<User> list_users(int offset, int count);

    // GET /v2.0/oa/conversation — toi da 10 tin nhan moi request voi moi user
    std::vector<Message> list_messages(const std::string& user_id,
                                       int offset, int count);

private:
    HttpClient& http_;
    std::string token_;
};

} // namespace zaloid

#pragma once
#include <string>
#include "zaloid/http_client.hpp"

namespace zaloid {

struct Tokens {
    std::string access_token;
    std::string refresh_token;
    long long expires_at = 0; // unix seconds
};

// OAuth v4 cua Zalo OA (PKCE): doi authorization code lay access token,
// va cap moi access token bang refresh token.
// Tai lieu: https://developers.zalo.me/docs/official-account/bat-dau/xac-thuc-va-uy-quyen-cho-ung-dung-new
class OAuth {
public:
    OAuth(HttpClient& http, std::string app_id, std::string secret_key);

    // PKCE: tao code verifier 43 ky tu va code challenge S256 tuong ung
    static std::string make_code_verifier();
    static std::string code_challenge_s256(const std::string& verifier);

    Tokens exchange_code(const std::string& code, const std::string& code_verifier);
    Tokens refresh(const std::string& refresh_token);

    static Tokens load(const std::string& path);
    static void save(const std::string& path, const Tokens& t);

private:
    Tokens request_token(const std::map<std::string, std::string>& form);

    HttpClient& http_;
    std::string app_id_;
    std::string secret_key_;
};

// Mo HTTP server tam tren 127.0.0.1:port, cho GET /callback?code=...
// Tra ve authorization code, hoac throw sau khi het timeout giay.
std::string wait_for_callback_code(int port, int timeout_sec);

} // namespace zaloid

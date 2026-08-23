#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include "zaloid/auth.hpp"
#include "zaloid/config.hpp"
#include "zaloid/http_client.hpp"
#include "zaloid/poller.hpp"
#include "zaloid/zalo_api.hpp"

namespace {
std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop.store(true); }

long long now_sec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// Luong login OAuth v4 cho OA (PKCE):
// 1) tao code_verifier/code_challenge  2) admin cau hinh tren developers.zalo.me
// 3) local server nhan authorization code  4) doi token va luu tokens.json
int cmd_login(const zaloid::Config& cfg, zaloid::HttpClient& http) {
    if (cfg.app_id.empty() || cfg.secret_key.empty()) {
        std::cerr << "Can app_id va secret_key trong config (hoac bien ZALO_APP_ID/ZALO_SECRET_KEY).\n";
        return 1;
    }
    zaloid::OAuth oauth(http, cfg.app_id, cfg.secret_key);

    std::string verifier = zaloid::OAuth::make_code_verifier();
    std::string challenge = zaloid::OAuth::code_challenge_s256(verifier);
    std::cout
        << "== Lua chon OAuth v4 (PKCE) ==\n"
        << "1. Mo https://developers.zalo.me -> ung dung cua ban -> phan cap quyen.\n"
        << "2. Dat callback URL: http://127.0.0.1:" << cfg.callback_port << "/callback\n"
        << "3. Dan code_challenge sau vao noi yeu cau, chon quyen \"Quan ly tin nhan nguoi dung\", luu:\n"
        << "   " << challenge << "\n"
        << "4. Copy duong dan cap quyen Zalo sinh ra, mo trong trinh duyet va chon \"Cho phep\"\n"
        << "   (trinh duyet se chuyen huong ve may ban)\n";

    std::string code = zaloid::wait_for_callback_code(cfg.callback_port, 300);

    zaloid::Tokens t = oauth.exchange_code(code, verifier);
    zaloid::OAuth::save(cfg.tokens_file, t);
    std::cout << "Da lay token thanh cong, luu vao " << cfg.tokens_file
              << " (access token het han sau " << (t.expires_at - now_sec()) / 3600 << " gio).\n";
    return 0;
}

// Lay access token: tu tokens.json (tu cap moi qua refresh token khi gan het han),
// hoac tu config/bien moi truong neu khong dung OAuth.
std::string resolve_access_token(const zaloid::Config& cfg, zaloid::HttpClient& http) {
    if (!cfg.app_id.empty() && !cfg.secret_key.empty()) {
        zaloid::OAuth oauth(http, cfg.app_id, cfg.secret_key);
        zaloid::Tokens t = zaloid::OAuth::load(cfg.tokens_file);
        if (t.access_token.empty() && t.refresh_token.empty()) {
            throw std::runtime_error(
                "Chua co token OAuth. Chay \"./zaloid login\" truoc, hoac dat access_token trong config.");
        }
        // cap moi som 5 phut truoc khi het han
        if (t.expires_at - 300 < now_sec()) {
            if (t.refresh_token.empty()) {
                throw std::runtime_error("Access token het han va khong co refresh_token - chay \"./zaloid login\".");
            }
            std::cout << "Access token gan het han, dang cap moi...\n";
            t = oauth.refresh(t.refresh_token);
            zaloid::OAuth::save(cfg.tokens_file, t);
        }
        return t.access_token;
    }
    return cfg.access_token;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    std::string config_path = "config.json";
    std::string command = "run";
    for (const auto& a : args) {
        if (a == "login") command = "login";
        else config_path = a;
    }
    try {
        zaloid::Config cfg = zaloid::Config::load(config_path);

        curl_global_init(CURL_GLOBAL_DEFAULT);
        struct CurlGlobal { ~CurlGlobal() { curl_global_cleanup(); } } cleanup;

        zaloid::HttpClient http;

        if (command == "login") return cmd_login(cfg, http);

        std::string token = resolve_access_token(cfg, http);
        zaloid::ZaloApi api(http, token);
        zaloid::Poller poller(api, cfg.poll_interval_sec, cfg.page_size, cfg.log_file);

        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);

        poller.run(g_stop);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Loi: " << e.what() << '\n';
        return 1;
    }
}

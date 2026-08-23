#include "zaloid/auth.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <cstring>
#include <fstream>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace zaloid {

namespace {

std::string url_encode_component(const std::string& s) {
    char* out = curl_easy_escape(nullptr, s.c_str(), static_cast<int>(s.size()));
    if (!out) throw std::runtime_error("Khong ma hoa duoc URL");
    std::string r = out;
    curl_free(out);
    return r;
}

std::string form_encode(const std::map<std::string, std::string>& fields) {
    std::string body;
    for (const auto& kv : fields) {
        if (!body.empty()) body += '&';
        body += url_encode_component(kv.first) + "=" + url_encode_component(kv.second);
    }
    return body;
}

// ---------- SHA-256 (FIPS 180-4), dung cho PKCE code challenge ----------

struct Sha256 {
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint64_t len = 0;
    unsigned char buf[64] = {};
    size_t buf_len = 0;

    static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void block(const unsigned char* p) {
        static const uint32_t k[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (uint32_t(p[4*i]) << 24) | (uint32_t(p[4*i+1]) << 16) |
                   (uint32_t(p[4*i+2]) << 8) | uint32_t(p[4*i+3]);
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i-15],7) ^ rotr(w[i-15],18) ^ (w[i-15] >> 3);
            uint32_t s1 = rotr(w[i-2],17) ^ rotr(w[i-2],19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
            uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + mj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }

    void update(const unsigned char* p, size_t n) {
        len += n;
        while (n > 0) {
            size_t take = std::min(n, 64 - buf_len);
            std::memcpy(buf + buf_len, p, take);
            buf_len += take; p += take; n -= take;
            if (buf_len == 64) { block(buf); buf_len = 0; }
        }
    }

    std::vector<unsigned char> finish() {
        uint64_t bits = len * 8;
        unsigned char pad = 0x80;
        update(&pad, 1);
        pad = 0;
        while (buf_len != 56) update(&pad, 1);
        unsigned char lenb[8];
        for (int i = 0; i < 8; ++i) lenb[i] = static_cast<unsigned char>(bits >> (56 - 8*i));
        update(lenb, 8);
        std::vector<unsigned char> out(32);
        for (int i = 0; i < 8; ++i) {
            out[4*i]   = static_cast<unsigned char>(h[i] >> 24);
            out[4*i+1] = static_cast<unsigned char>(h[i] >> 16);
            out[4*i+2] = static_cast<unsigned char>(h[i] >> 8);
            out[4*i+3] = static_cast<unsigned char>(h[i]);
        }
        return out;
    }
};

const char kBase64Url[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string base64url_nopad(const std::vector<unsigned char>& data) {
    std::string out;
    size_t i = 0;
    while (i + 2 < data.size()) {
        uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i+1]) << 8) | data[i+2];
        out += kBase64Url[(v >> 18) & 63]; out += kBase64Url[(v >> 12) & 63];
        out += kBase64Url[(v >> 6) & 63];  out += kBase64Url[v & 63];
        i += 3;
    }
    size_t rem = data.size() - i;
    if (rem == 1) {
        uint32_t v = uint32_t(data[i]) << 16;
        out += kBase64Url[(v >> 18) & 63]; out += kBase64Url[(v >> 12) & 63];
    } else if (rem == 2) {
        uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i+1]) << 8);
        out += kBase64Url[(v >> 18) & 63]; out += kBase64Url[(v >> 12) & 63];
        out += kBase64Url[(v >> 6) & 63];
    }
    return out;
}

long long now_sec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

OAuth::OAuth(HttpClient& http, std::string app_id, std::string secret_key)
    : http_(http), app_id_(std::move(app_id)), secret_key_(std::move(secret_key)) {}

std::string OAuth::make_code_verifier() {
    // 43 ky tu, co du chu hoa, chu thuong, chu so (yeu cau cua Zalo)
    static const char upper[] = "ABCDEFGHJKLMNPQRSTUVWXYZ";
    static const char lower[] = "abcdefghijkmnpqrstuvwxyz";
    static const char digit[] = "23456789";
    std::random_device rd;
    std::string v;
    v += upper[rd() % (sizeof(upper) - 1)];
    v += lower[rd() % (sizeof(lower) - 1)];
    v += digit[rd() % (sizeof(digit) - 1)];
    static const char all[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    while (v.size() < 43) v += all[rd() % (sizeof(all) - 1)];
    return v;
}

std::string OAuth::code_challenge_s256(const std::string& verifier) {
    Sha256 sha;
    sha.update(reinterpret_cast<const unsigned char*>(verifier.data()), verifier.size());
    return base64url_nopad(sha.finish());
}

Tokens OAuth::request_token(const std::map<std::string, std::string>& form) {
    HttpResponse resp = http_.post("https://oauth.zaloapp.com/v4/oa/access_token",
                                   form_encode(form),
                                   {{"Content-Type", "application/x-www-form-urlencoded"},
                                    {"secret_key", secret_key_}});
    nlohmann::json j = nlohmann::json::parse(resp.body, nullptr, false);
    if (j.is_discarded()) throw std::runtime_error("Phan hoi OAuth khong phai JSON: " + resp.body.substr(0, 200));
    if (j.contains("error") && !j["error"].is_number_integer()) {
        // OAuth v4 tra loi o dang {"error_name":..., "message":...} khi that bai
        throw std::runtime_error("OAuth loi: " + j.value("error_name", "") + ": " + j.value("message", ""));
    }
    if (j.contains("error") && j["error"].is_number_integer() && j["error"].get<int>() != 0) {
        throw std::runtime_error("OAuth loi " + std::to_string(j["error"].get<int>()) + ": " + j.value("message", ""));
    }
    if (!j.contains("access_token")) throw std::runtime_error("Phan hoi OAuth thieu access_token");
    Tokens t;
    t.access_token = j["access_token"].get<std::string>();
    t.refresh_token = j.value("refresh_token", "");
    long long expires_in = 90000; // mac dinh 25h
    if (j.contains("expires_in")) {
        if (j["expires_in"].is_string()) expires_in = std::stoll(j["expires_in"].get<std::string>());
        else expires_in = j["expires_in"].get<long long>();
    }
    t.expires_at = now_sec() + expires_in;
    return t;
}

Tokens OAuth::exchange_code(const std::string& code, const std::string& code_verifier) {
    return request_token({{"code", code},
                          {"app_id", app_id_},
                          {"grant_type", "authorization_code"},
                          {"code_verifier", code_verifier}});
}

Tokens OAuth::refresh(const std::string& refresh_token) {
    return request_token({{"refresh_token", refresh_token},
                          {"app_id", app_id_},
                          {"grant_type", "refresh_token"}});
}

Tokens OAuth::load(const std::string& path) {
    Tokens t;
    std::ifstream in(path);
    if (!in) return t;
    nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_discarded()) return t;
    t.access_token = j.value("access_token", "");
    t.refresh_token = j.value("refresh_token", "");
    t.expires_at = j.value("expires_at", 0LL);
    return t;
}

void OAuth::save(const std::string& path, const Tokens& t) {
    nlohmann::json j = {{"access_token", t.access_token},
                        {"refresh_token", t.refresh_token},
                        {"expires_at", t.expires_at}};
    std::ofstream out(path);
    out << j.dump(2) << '\n';
    if (!out) throw std::runtime_error("Khong ghi duoc " + path);
}

std::string wait_for_callback_code(int port, int timeout_sec) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) throw std::runtime_error("WSAStartup that bai");
#endif
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("Khong tao duoc socket");
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(uint16_t(port));
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
        listen(fd, 1) < 0) {
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        throw std::runtime_error("Khong mo duoc cong 127.0.0.1:" + std::to_string(port));
    }

    timeval tv{timeout_sec, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::cout << "Dang cho callback tren http://127.0.0.1:" << port << "/callback "
              << "(toi da " << timeout_sec << " giay)...\n";

    std::string code;
    int conn = accept(fd, nullptr, nullptr);
    if (conn >= 0) {
        char buf[4096];
        long n = recv(conn, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            std::string req(buf);
            size_t q = req.find("code=");
            if (q != std::string::npos) {
                size_t e = req.find_first_of(" &", q + 5);
                code = req.substr(q + 5, e == std::string::npos ? std::string::npos : e - (q + 5));
            }
        }
        const char* ok =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
            "Connection: close\r\n\r\n"
            "<html><body><h3>Da nhan authorization code - co the dong cua so nay.</h3></body></html>";
        send(conn, ok, int(std::strlen(ok)), 0);
#ifdef _WIN32
        closesocket(conn);
        closesocket(fd);
#else
        close(conn);
        close(fd);
#endif
    }
    if (code.empty()) throw std::runtime_error("Khong nhan duoc authorization code trong callback");
    return code;
}

} // namespace zaloid

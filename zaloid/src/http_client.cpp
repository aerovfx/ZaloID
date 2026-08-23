#include "zaloid/http_client.hpp"
#include <curl/curl.h>
#include <stdexcept>

namespace zaloid {

namespace {

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::string*>(userdata);
    body->append(ptr, size * nmemb);
    return size * nmemb;
}

struct HeaderList {
    curl_slist* list = nullptr;
    ~HeaderList() { if (list) curl_slist_free_all(list); }
};

HttpResponse perform(CURL* curl, const std::string& url,
                     const std::map<std::string, std::string>& headers,
                     const std::string* post_body) {
    HttpResponse resp;
    HeaderList hl;
    for (const auto& kv : headers) {
        hl.list = curl_slist_append(hl.list, (kv.first + ": " + kv.second).c_str());
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hl.list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    if (post_body) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body->c_str());
    }
    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        throw std::runtime_error(std::string("Loi mang: ") + curl_easy_strerror(rc));
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);
    return resp;
}

} // namespace

HttpResponse HttpClient::get(const std::string& url,
                             const std::map<std::string, std::string>& headers) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Khong khoi tao duoc curl");
    struct Guard { CURL* c; ~Guard() { curl_easy_cleanup(c); } } guard{curl};
    return perform(curl, url, headers, nullptr);
}

HttpResponse HttpClient::post(const std::string& url, const std::string& body,
                              const std::map<std::string, std::string>& headers) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Khong khoi tao duoc curl");
    struct Guard { CURL* c; ~Guard() { curl_easy_cleanup(c); } } guard{curl};
    return perform(curl, url, headers, &body);
}

} // namespace zaloid

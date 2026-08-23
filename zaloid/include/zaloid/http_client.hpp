#pragma once
#include <map>
#include <string>

namespace zaloid {

struct HttpResponse {
    long status = 0;
    std::string body;
    bool ok() const { return status >= 200 && status < 300; }
};

class HttpClient {
public:
    HttpResponse get(const std::string& url,
                     const std::map<std::string, std::string>& headers = {});
    HttpResponse post(const std::string& url, const std::string& body,
                      const std::map<std::string, std::string>& headers = {});
};

} // namespace zaloid

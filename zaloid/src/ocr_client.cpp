#include "zaloid/ocr_client.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace zaloid {

namespace {

// Base64 encoding
static const char* b64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const unsigned char* data, size_t len) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (len--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++) ret += b64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        for (j = 0; j < i + 1; j++) ret += b64_chars[char_array_4[j]];
        while (i++ < 3) ret += '=';
    }
    return ret;
}

std::string base64_encode(const std::string& data) {
    return base64_encode(reinterpret_cast<const unsigned char*>(data.c_str()), data.size());
}

// Callback for curl download
size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

// Download image from URL
std::string download_image(CURL* curl, const std::string& url, int timeout_sec, size_t max_bytes) {
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE, static_cast<curl_off_t>(max_bytes));
    
    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        throw std::runtime_error("Download failed: " + std::string(curl_easy_strerror(rc)));
    }
    
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code < 200 || http_code >= 300) {
        throw std::runtime_error("HTTP error: " + std::to_string(http_code));
    }
    
    return response;
}

} // anonymous namespace

OcrClient::OcrClient(HttpClient& http, const OcrConfig& config)
    : http_(http), config_(config) {}

std::string OcrClient::get_profile_prompt(AnalysisProfile profile) {
    switch (profile) {
        case AnalysisProfile::Document:
            return R"(Bạn là chuyên viên phân tích văn bản hành chính. Hãy trích xuất thông tin từ ảnh văn bản sau theo cấu trúc JSON:

{
  "summary": "Tóm tắt 2-3 câu",
  "people": ["Tên người 1", "Tên người 2"],
  "decision_maker": "Tên người ký/ban hành",
  "work_content": "Nội dung chính của văn bản",
  "security_level": "Công khai/Nội bộ/Mật/Tuyệt mật",
  "document_type": "Quyết định/Thông báo/Công văn/Chỉ thị/...",
  "issue_date": "DD/MM/YYYY",
  "reference_number": "Số: XX/YYYY/QLĐT",
  "keywords": ["từ khóa 1", "từ khóa 2"],
  "raw_ocr_text": "Toàn bộ văn bản OCR được"
}

Yêu cầu:
- Trích NGUYÊN VĂN các trường quan trọng (số ký hiệu, ngày, tên người ký)
- Mức độ bảo mật: tìm từ "Mật", "Tuyệt mật", "Nội bộ", "Công khai" trong văn bản
- Nếu không xác định được: để trống hoặc "Không rõ"
- Chỉ trả về JSON, KHÔNG thêm giải thích)";
        
        case AnalysisProfile::Invoice:
            return R"(Bạn là chuyên viên trích xuất dữ liệu hóa đơn. Phân tích ảnh hóa đơn/biên lai và trả về JSON:

{
  "summary": "Mô tả ngắn",
  "people": ["Người bán", "Người mua"],
  "decision_maker": "",
  "work_content": "Danh sách mặt hàng/dịch vụ",
  "security_level": "",
  "document_type": "Hóa đơn/Biên lai",
  "issue_date": "DD/MM/YYYY",
  "reference_number": "Số hóa đơn",
  "keywords": ["tổng tiền", "thuế", "phương thức thanh toán"],
  "raw_ocr_text": "Toàn bộ văn bản"
}

Trích xuất: Tên người bán/mua, mã số thuế, địa chỉ, số hóa đơn, ngày, bảng mặt hàng (tên, SL, đơn giá, thành tiền), tạm tính, thuế/VAT, giảm giá, tổng cộng, phương thức thanh toán.)";
        
        case AnalysisProfile::Report:
            return R"(Bạn là chuyên viên phân tích báo cáo/biểu đồ. Phân tích ảnh báo cáo và trả về JSON:

{
  "summary": "Tóm tắt kết quả chính",
  "people": [],
  "decision_maker": "",
  "work_content": "Các chỉ tiêu, số liệu, xu hướng chính",
  "security_level": "",
  "document_type": "Báo cáo/Biểu đồ",
  "issue_date": "",
  "reference_number": "",
  "keywords": ["chỉ tiêu 1", "chỉ tiêu 2"],
  "raw_ocr_text": "Toàn bộ số liệu và chữ"
}

Trích xuất: Loại biểu đồ/bảng, tiêu đề, nhãn trục, đơn vị, chú giải, dữ liệu số liệu chính xác, xu hướng, nhận xét/kết luận.)";
        
        case AnalysisProfile::General:
        default:
            return R"(Bạn là trợ lý phân tích hình ảnh. Phân tích ảnh và trả về JSON:

{
  "summary": "Mô tả nội dung ảnh",
  "people": ["Nhân vật nếu có"],
  "decision_maker": "",
  "work_content": "Nội dung chính",
  "security_level": "",
  "document_type": "Ảnh chung",
  "issue_date": "",
  "reference_number": "",
  "keywords": ["từ khóa"],
  "raw_ocr_text": "Toàn bộ chữ trong ảnh"
}

Mô tả: nhân vật, bối cảnh, diễn biến, trích xuất toàn bộ chữ.)";
    }
}

double OcrClient::get_profile_temperature(AnalysisProfile profile) {
    switch (profile) {
        case AnalysisProfile::Document:
        case AnalysisProfile::Invoice:
        case AnalysisProfile::Report:
            return 0.1;  // High accuracy needed
        case AnalysisProfile::General:
        default:
            return 0.2;
    }
}

std::string OcrClient::download_and_encode(const std::string& url) const {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Cannot initialize curl");
    
    struct CurlGuard { CURL* c; ~CurlGuard() { curl_easy_cleanup(c); } } guard{curl};
    
    size_t max_bytes = static_cast<size_t>(config_.max_image_size_mb) * 1024 * 1024;
    std::string image_data = download_image(curl, url, config_.timeout_sec, max_bytes);
    
    return base64_encode(image_data);
}

std::string OcrClient::build_payload(const std::vector<std::string>& base64_images, const std::string& prompt, double temperature) const {
    nlohmann::json payload;
    payload["model"] = config_.model;
    payload["prompt"] = prompt;
    payload["images"] = base64_images;
    payload["stream"] = false;
    payload["options"]["temperature"] = temperature;
    return payload.dump();
}

DocumentAnalysis OcrClient::call_ollama(const std::vector<std::string>& base64_images, const std::string& prompt, double temperature) const {
    std::string payload = build_payload(base64_images, prompt, temperature);
    
    std::map<std::string, std::string> headers = {{"Content-Type", "application/json"}};
    HttpResponse resp = http_.post(config_.endpoint, payload, headers);
    
    if (!resp.ok()) {
        throw std::runtime_error("Ollama HTTP error: " + std::to_string(resp.status) + " - " + resp.body.substr(0, 200));
    }
    
    return parse_response(resp.body);
}

DocumentAnalysis OcrClient::parse_response(const std::string& response) const {
    DocumentAnalysis result;
    
    try {
        nlohmann::json j = nlohmann::json::parse(response, nullptr, false);
        if (j.is_discarded()) {
            result.error_message = "Invalid JSON response: " + response.substr(0, 200);
            return result;
        }
        
        // Ollama returns: {"response": "...", "done": true, ...}
        std::string content = j.value("response", "");
        if (content.empty()) {
            result.error_message = "Empty response from Ollama";
            return result;
        }
        
        // Try to parse the content as JSON (our prompt asks for JSON)
        nlohmann::json content_json = nlohmann::json::parse(content, nullptr, false);
        if (!content_json.is_discarded()) {
            result.summary = content_json.value("summary", "");
            result.people = content_json.value("people", std::vector<std::string>{});
            result.decision_maker = content_json.value("decision_maker", "");
            result.work_content = content_json.value("work_content", "");
            result.security_level = content_json.value("security_level", "");
            result.document_type = content_json.value("document_type", "");
            result.issue_date = content_json.value("issue_date", "");
            result.reference_number = content_json.value("reference_number", "");
            result.keywords = content_json.value("keywords", std::vector<std::string>{});
            result.raw_ocr_text = content_json.value("raw_ocr_text", "");
            result.success = true;
        } else {
            // Fallback: treat as plain text
            result.raw_ocr_text = content;
            result.summary = content.substr(0, 200);
            result.success = true;
        }
    } catch (const std::exception& e) {
        result.error_message = "Parse error: " + std::string(e.what());
        result.raw_ocr_text = response.substr(0, 500);
    }
    
    return result;
}

DocumentAnalysis OcrClient::analyze_image(const ImageData& image, AnalysisProfile profile) {
    DocumentAnalysis result;
    
    if (!config_.enabled) {
        result.error_message = "OCR is disabled in config";
        return result;
    }
    
    try {
        std::string base64_image;
        if (!image.base64_data.empty()) {
            base64_image = image.base64_data;
        } else if (!image.url.empty() && config_.download_images) {
            base64_image = download_and_encode(image.url);
        } else {
            result.error_message = "No image data or URL provided";
            return result;
        }
        
        std::string prompt = get_profile_prompt(profile);
        double temp = (profile == AnalysisProfile::Document) ? config_.temperature : get_profile_temperature(profile);
        
        result = call_ollama({base64_image}, prompt, temp);
        
        if (!result.success && result.error_message.empty()) {
            result.error_message = "Unknown error during analysis";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
    }
    
    return result;
}

std::vector<DocumentAnalysis> OcrClient::analyze_images(const std::vector<ImageData>& images, AnalysisProfile profile) {
    std::vector<DocumentAnalysis> results;
    results.reserve(images.size());
    
    for (const auto& img : images) {
        results.push_back(analyze_image(img, profile));
    }
    
    return results;
}

DocumentAnalysis OcrClient::analyze_with_prompt(const ImageData& image, const std::string& custom_prompt, double temperature) {
    DocumentAnalysis result;
    
    if (!config_.enabled) {
        result.error_message = "OCR is disabled in config";
        return result;
    }
    
    try {
        std::string base64_image;
        if (!image.base64_data.empty()) {
            base64_image = image.base64_data;
        } else if (!image.url.empty() && config_.download_images) {
            base64_image = download_and_encode(image.url);
        } else {
            result.error_message = "No image data or URL provided";
            return result;
        }
        
        double temp = (temperature >= 0) ? temperature : config_.temperature;
        result = call_ollama({base64_image}, custom_prompt, temp);
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
    }
    
    return result;
}

bool OcrClient::check_service() const {
    try {
        // Try to call /api/tags to check if Ollama is running
        std::string tags_url = config_.endpoint;
        size_t pos = tags_url.find("/api/generate");
        if (pos != std::string::npos) {
            tags_url.replace(pos, std::string("/api/generate").length(), "/api/tags");
        } else {
            tags_url = "http://localhost:11434/api/tags";
        }
        
        HttpResponse resp = http_.get(tags_url, {});
        return resp.ok();
    } catch (...) {
        return false;
    }
}

std::vector<std::string> OcrClient::list_models() const {
    std::vector<std::string> models;
    try {
        std::string tags_url = config_.endpoint;
        size_t pos = tags_url.find("/api/generate");
        if (pos != std::string::npos) {
            tags_url.replace(pos, std::string("/api/generate").length(), "/api/tags");
        } else {
            tags_url = "http://localhost:11434/api/tags";
        }
        
        HttpResponse resp = http_.get(tags_url, {});
        if (resp.ok()) {
            nlohmann::json j = nlohmann::json::parse(resp.body, nullptr, false);
            if (!j.is_discarded() && j.contains("models")) {
                for (const auto& m : j["models"]) {
                    models.push_back(m.value("name", ""));
                }
            }
        }
    } catch (...) {
        // Ignore errors
    }
    return models;
}

} // namespace zaloid

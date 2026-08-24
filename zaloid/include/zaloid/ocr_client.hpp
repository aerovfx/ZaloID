#pragma once
#include <string>
#include <vector>
#include <map>
#include "zaloid/http_client.hpp"

namespace zaloid {

// Forward declaration
class HttpClient;

struct OcrConfig {
    std::string endpoint = "http://localhost:11434/api/generate";
    std::string model = "qwen2.5vl:7b";
    int timeout_sec = 120;
    double temperature = 0.1;
    bool enabled = true;
    std::string default_profile = "document";
    bool download_images = true;
    int max_image_size_mb = 10;
};

struct ImageData {
    std::string url;           // Image URL to download
    std::string base64_data;   // Pre-encoded base64 (optional)
    std::string mime_type;     // image/jpeg, image/png, etc.
    std::string id;            // Identifier for logging
};

struct DocumentAnalysis {
    std::string summary;
    std::vector<std::string> people;
    std::string decision_maker;
    std::string work_content;
    std::string security_level;
    std::string document_type;
    std::string issue_date;
    std::string reference_number;
    std::vector<std::string> keywords;
    std::string raw_ocr_text;
    bool success = false;
    std::string error_message;
};

// Analysis profile types
enum class AnalysisProfile {
    Document,    // Văn bản hành chính, công văn
    Invoice,     // Hóa đơn, biên lai
    Report,      // Báo cáo, biểu đồ
    General      // Tổng quát
};

class OcrClient {
public:
    explicit OcrClient(HttpClient& http, const OcrConfig& config);

    // Analyze a single image with specified profile
    DocumentAnalysis analyze_image(const ImageData& image, AnalysisProfile profile = AnalysisProfile::Document);

    // Analyze multiple images (e.g., multi-page document)
    std::vector<DocumentAnalysis> analyze_images(const std::vector<ImageData>& images, AnalysisProfile profile = AnalysisProfile::Document);

    // Analyze with custom prompt
    DocumentAnalysis analyze_with_prompt(const ImageData& image, const std::string& custom_prompt, double temperature = -1.0);

    // Check if OCR service is available
    bool check_service() const;

    // Get available models from Ollama
    std::vector<std::string> list_models() const;

    // Get config (read-only access)
    const OcrConfig& get_config() const { return config_; }

private:
    HttpClient& http_;
    OcrConfig config_;

    // Profile-specific prompts
    static std::string get_profile_prompt(AnalysisProfile profile);
    static double get_profile_temperature(AnalysisProfile profile);

    // Download image and convert to base64
    std::string download_and_encode(const std::string& url) const;

    // Call Ollama API
    DocumentAnalysis call_ollama(const std::vector<std::string>& base64_images, const std::string& prompt, double temperature) const;

    // Parse JSON response from Ollama
    DocumentAnalysis parse_response(const std::string& response) const;

    // Build JSON payload for Ollama
    std::string build_payload(const std::vector<std::string>& base64_images, const std::string& prompt, double temperature) const;
};

} // namespace zaloid

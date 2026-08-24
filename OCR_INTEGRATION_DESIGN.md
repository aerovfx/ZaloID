# OCR/Image Analysis Integration for zaloid

## Overview
Integrate OCR and image understanding capabilities into zaloid to automatically extract structured information from images sent to Zalo OA. Reference: AeroMD's `img.sh` + `vision.sh` using Ollama + Qwen2.5-VL.

## Architecture

### Current Flow
Zalo OA → Poller → Messages (text only) → Log

### New Flow
Zalo OA → Poller → Messages (text + images) → OCR Service → Structured Analysis → Enhanced Log

## OCR Service Options

| Option | Pros | Cons |
|--------|------|------|
| **Ollama + Qwen2.5-VL (AeroMD style)** | Local, private, free, good Vietnamese support | Needs Ollama installed, GPU recommended |
| **Tesseract (C++ lib)** | Native C++, no external service | Poor Vietnamese, no layout understanding |
| **PaddleOCR C++ API** | Good accuracy, C++ API | Complex build, large deps |
| **Cloud API (Google Vision, AWS)** | High accuracy | Cost, privacy, network dependency |

**Decision**: Use **Ollama + Qwen2.5-VL** (same as AeroMD) — local, private, supports Vietnamese, handles complex documents (tables, forms, handwriting).

## Component Design

### 1. OCR Client (`include/zaloid/ocr_client.hpp`, `src/ocr_client.cpp`)
- HTTP client wrapper for Ollama `/api/generate` endpoint
- Base64 encode images
- Support multiple analysis profiles (document, invoice, report, general)
- Configurable model, temperature, timeout

### 2. Extended Message Struct (`include/zaloid/zalo_api.hpp`)
```cpp
struct ImageAttachment {
    std::string id;           // Zalo media ID
    std::string url;          // Direct image URL
    std::string mime_type;    // image/jpeg, image/png
    int width = 0;
    int height = 0;
    long long size = 0;       // bytes
};

struct Message {
    std::string id;
    std::string from;
    std::string from_name;
    std::string text;
    long long created_at = 0;
    std::vector<ImageAttachment> images;  // NEW
};
```### 3. Document Analysis Result (`include/zaloid/document_analysis.hpp`)
```cpp
struct DocumentAnalysis {
    std::string summary;              // Tóm tắt ngắn
    std::vector<std::string> people;  // Người liên quan
    std::string decision_maker;       // Người cấp quyết định
    std::string work_content;         // Nội dung công việc
    std::string security_level;       // Mức độ bảo mật (Công khai/Nội bộ/Mật/...)
    std::string document_type;        // Loại văn bản (Quyết định/Thông báo/Công văn/...)
    std::string issue_date;           // Ngày ban hành
    std::string reference_number;     // Số ký hiệu
    std::vector<std::string> keywords; // Từ khóa
    std::string raw_ocr_text;         // Full OCR text for reference
    bool success = false;
    std::string error_message;
};
```

### 4. Analysis Profiles (Prompts)
| Profile | Use Case | Prompt Focus |
|---------|----------|--------------|
| `document` | Văn bản hành chính, công văn | Cấu trúc, số ký hiệu, người ký, nội dung, mật độ |
| `invoice` | Hóa đơn, biên lai | Người bán/mua, số tiền, mặt hàng, thuế |
| `report` | Báo cáo, biểu đồ | Số liệu, xu hướng, kết luận |
| `general` | Ảnh chung | Mô tả chung, trích xuất chữ |

### 5. Config Extensions (`config.example.json`)
```json
{
  "ocr_enabled": true,
  "ocr_endpoint": "http://localhost:11434/api/generate",
  "ocr_model": "qwen2.5vl:7b",
  "ocr_timeout_sec": 120,
  "ocr_default_profile": "document",
  "ocr_download_images": true,
  "ocr_max_image_size_mb": 10
}
```

## Implementation Steps

1. **Add OCR client** - HTTP wrapper for Ollama vision API
2. **Extend Message struct** - Add image attachments
3. **Update ZaloApi** - Fetch image info from messages (need to check Zalo API for image data)
4. **Add DocumentAnalysis struct** - Structured output
5. **Integrate into Poller** - Process images after receiving messages
6. **Update config** - Add OCR settings
7. **Update CMakeLists** - Add new source files

## Zalo API Image Handling

Need to check: Zalo OA conversation API returns message with `type` field. For images:
- `type` = "image"
- `message` field may contain image URL or media ID
- May need separate API call to get image URL

## Prompt Templates (Vietnamese)

### Document Profile (Main)
```
Bạn là chuyên viên phân tích văn bản hành chính. Hãy trích xuất thông tin từ ảnh văn bản sau theo cấu trúc JSON:

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
- Chỉ trả về JSON, KHÔNG thêm giải thích
```

## File Structure After Integration

```
zaloid/
├── include/zaloid/
│   ├── ocr_client.hpp          # NEW
│   ├── document_analysis.hpp   # NEW
│   ├── zalo_api.hpp            # MODIFIED (Message + ImageAttachment)
│   ├── config.hpp              # MODIFIED (OCR config)
│   └── poller.hpp              # MODIFIED (OCR integration)
├── src/
│   ├── ocr_client.cpp          # NEW
│   ├── zalo_api.cpp            # MODIFIED
│   ├── config.cpp              # MODIFIED
│   ├── poller.cpp              # MODIFIED
│   └── main.cpp                # MODIFIED (pass OCR config)
└── CMakeLists.txt              # MODIFIED
```

## Dependencies

- Existing: libcurl, nlohmann/json (already used)
- New runtime dependency: **Ollama** with **qwen2.5vl:7b** model
- No new compile-time dependencies

## Testing Plan

1. Unit test: OCR client with mock HTTP server
2. Integration test: Send test image to local Ollama
3. End-to-end: Simulate Zalo message with image → check structured output

# zaloid

Giới thiệu đến các bạn ứng dụng C++17 tự động lấy tin nhắn từ **Zalo Official Account (OA)** qua Zalo Open API chính thức (`openapi.zalo.me`). Nó là module ghép nối vào app để chúng ta lên kế hoạch thực hiện nhiệm vụ với các công việc được giao trong Zalo.

Ứng dụng poll định kỳ: lấy danh sách user đã tương tác với OA (`GET /v3.0/oa/user/getlist`), rồi lấy tối đa 10 tin nhắn gần nhất của từng user (`GET /v2.0/oa/conversation`), chống trùng theo `message_id`, in ra console và ghi vào file log.

**Tính năng mới: OCR/Image Analysis** — Tự động nhận diện và phân tích hình ảnh/ảnh chụp văn bản từ tin nhắn Zalo, trích xuất thông tin có cấu trúc: người liên quan, người cấp quyết định, nội dung công việc, mức độ bảo mật, số ký hiệu, ngày ban hành, v.v. Sử dụng mô hình **Qwen2.5-VL** chạy offline qua **Ollama** (tham khảo từ AeroMD).

![Sơ đồ luồng hoạt động của zaloid](zaloID.png)

*Sơ đồ luồng hoạt động: đăng nhập OA (OAuth v4 + PKCE, tự làm mới token) → poll danh sách user đã tương tác → lấy tin nhắn mới của từng user → **OCR phân tích ảnh** → chống trùng theo `message_id` → in ra console và ghi `zaloid.log`.*

---

## Yêu cầu hệ thống

### Bắt buộc
- **CMake ≥ 3.14**
- **Trình biên dịch C++17** (GCC ≥ 7, Clang ≥ 5, MSVC ≥ 2017)
- **libcurl** (development headers)
- **nlohmann/json** — tự động tải qua FetchContent khi build lần đầu (cần mạng)

### Tùy chọn (cho OCR)
- **Ollama** + model **qwen2.5vl:7b** — chạy offline, riêng tư, miễn phí

---

## Cài đặt phụ thuộc theo hệ điều hành

### macOS (Apple Silicon / Intel)

```bash
# Cài Homebrew nếu chưa có
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Cài CMake, libcurl
brew install cmake curl

# Cài Ollama cho OCR (khuyến nghị)
brew install ollama
ollama serve &                    # Chạy nền (hoặc mở app Ollama)
ollama pull qwen2.5vl:7b          # Tải model (~4.7GB)
```

### Ubuntu / Debian

```bash
sudo apt update
sudo apt install -y cmake g++ libcurl4-openssl-dev

# Cài Ollama cho OCR
curl -fsSL https://ollama.com/install.sh | sh
ollama serve &
ollama pull qwen2.5vl:7b
```

### CentOS / RHEL / Fedora

```bash
sudo dnf install -y cmake gcc-c++ libcurl-devel

# Hoặc CentOS 7:
# sudo yum install -y cmake3 gcc-c++ libcurl-devel
# ln -sf /usr/bin/cmake3 /usr/local/bin/cmake

# Cài Ollama
curl -fsSL https://ollama.com/install.sh | sh
ollama serve &
ollama pull qwen2.5vl:7b
```

### Windows (MSVC / MinGW)

**Yêu cầu:** Visual Studio 2019+ với workload "Desktop development with C++" **HOẶC** MinGW-w64 qua MSYS2/Chocolatey.

```powershell
# Cài CMake qua Chocolatey
choco install cmake --installargs 'ADD_CMAKE_TO_PATH=System'

# Cài libcurl (vcpkg khuyến nghị)
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg integrate install
.\vcpkg\vcpkg install curl:x64-windows

# Cài Ollama
winget install Ollama.Ollama
ollama serve
ollama pull qwen2.5vl:7b
```

> **Lưu ý Windows:** Build với MSVC cần vcpkg để tìm libcurl. MinGW cần `pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc mingw-w64-x86_64-curl`.

---

## Build dự án

```bash
cd zaloid                                    # Vào thư mục source (chứa CMakeLists.txt)
cmake -S . -B build                          # Cấu hình (tạo build/)
cmake --build build -j$(nproc)               # Build đa luồng (Linux/macOS)
# Hoặc Windows PowerShell:
# cmake --build build --config Release
```

**Kết quả:** File thực thi `build/zaloid` (hoặc `build/Release/zaloid.exe` trên Windows).

### Build tùy chọn

```bash
# Build Release (tối ưu tốc độ)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Build Debug (có symbol debug)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Chỉ định compiler cụ thể
cmake -S . -B build -DCMAKE_CXX_COMPILER=clang++
cmake --build build

# Cross-compile (ví dụ Linux → Windows qua MinGW)
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=../mingw-toolchain.cmake
cmake --build build
```

---

## Cài đặt Ollama cho OCR (Chi tiết)

### macOS
```bash
brew install ollama
ollama serve &
ollama pull qwen2.5vl:7b

# Kiểm tra
ollama list
# NAME            ID              SIZE      MODIFIED
# qwen2.5vl:7b    abc123          4.7 GB    2 minutes ago
```

### Linux (systemd service - khuyến nghị cho server)
```bash
curl -fsSL https://ollama.com/install.sh | sh
sudo systemctl enable --now ollama
ollama pull qwen2.5vl:7b

# Kiểm tra service
systemctl status ollama
curl http://localhost:11434/api/tags
```

### Docker (cách nhanh nhất, không cài cắm hệ thống)
```bash
# Chạy Ollama trong container
docker run -d --name ollama -p 11434:11434 -v ollama:/root/.ollama ollama/ollama
docker exec ollama ollama pull qwen2.5vl:7b

# Kiểm tra
curl http://localhost:11434/api/tags
```

### Model nhẹ hơn (nếu RAM ít)
```bash
# qwen2.5vl:3b (~2.3GB) - nhanh hơn, độ chính xác hơi kém
ollama pull qwen2.5vl:3b

# Cập nhật config.json: "model": "qwen2.5vl:3b"
```

---

## Lấy Access Token Zalo OA

Zalo **KHÔNG có API cho tài khoản cá nhân** — bắt buộc có **Official Account (OA)**.

### Bước 1: Tạo ứng dụng trên Zalo Developers
1. Truy cập [developers.zalo.me](https://developers.zalo.me), đăng nhập bằng tài khoản quản trị OA.
2. Tạo ứng dụng mới → Lấy **App ID** và **Secret Key**.
3. Cấu hình quyền: **Quản lý tin nhắn/hội thoại của OA** (Message/Conversation management).

### Bước 2: Chọn cách cấp token

#### Cách 1: Token tĩnh (đơn giản, phù hợp test)
- Vào ứng dụng trên developers.zalo.me → Copy **OA Access Token** (hoặc Long-term Token).
- Dán vào `config.json` hoặc set biến môi trường:

```bash
export ZALO_ACCESS_TOKEN="your_oa_access_token_here"
```

#### Cách 2: OAuth v4 + PKCE (tự động làm mới token — khuyến nghị production)
1. Điền `app_id` và `secret_key` vào `config.json` (hoặc set `ZALO_APP_ID`, `ZALO_SECRET_KEY`).
2. Chạy lệnh login:

```bash
./build/zaloid login
```

3. Làm theo hướng dẫn trên màn hình:
   - Mở https://developers.zalo.me → Ứng dụng của bạn → Phần Cấp quyền
   - **Callback URL:** `http://127.0.0.1:18080/callback`
   - **Code Challenge:** (copy từ output chương trình)
   - Chọn quyền: **Quản lý tin nhắn người dùng**
   - Mở đường dẫn cấp quyền Zalo sinh ra → Chọn "Cho phép"
   - Trình duyệt sẽ redirect về localhost, chương trình tự lấy token

4. Token được lưu vào `tokens.json` và **tự động refresh** mỗi khi gần hết hạn (25 giờ).

> **Lưu ý:** Refresh token hạn 3 tháng, chỉ dùng 1 lần. Mỗi lần refresh sinh token mới, app tự lưu đè. Đừng tắt máy quá 3 tháng giữa 2 lần chạy.

---

## Chạy ứng dụng

### Chế độ bình thường (poll liên tục)

```bash
# Dùng config.json ở thư mục hiện tại
./build/zaloid

# Hoặc chỉ định file config tùy chỉnh
./build/zaloid /path/to/config.json
```

**Output mẫu:**
```
Bat dau poll moi 10 giay...
[OCR] Ket noi Ollama thanh cong (model: qwen2.5vl:7b)
[2024-01-15 14:30:22] Nguyen Van A: Xin chao, day la van ban quyet dinh moi
[2024-01-15 14:30:25] Tran Thi B [OCR PHAN TICH]: Tom tat: Quyet dinh ve viec phong ban nhan su | Loai: Quyet dinh | So ky hieu: 123/QD-NS | Ngay: 15/01/2024 | Nguoi ky: Tran Thi B (Giam doc) | Do mat: Noi bo | Noi dung: Bo sung chuc danh, dieu chinh luong | Nguoi lien quan: Nguyen Van A, Tran Thi B | Tu khoa: nhan su, luong, chuc danh
[2024-01-15 14:30:30] Le Van C: Da nhan van ban
```

### Chế độ đăng nhập OAuth

```bash
./build/zaloid login
```

### Dừng ứng dụng

Nhấn **Ctrl+C** (gửi SIGINT) để dừng an toàn. Ứng dụng sẽ flush log và thoát.

---

## Cấu hình (config.json)

Copy file mẫu và chỉnh sửa:

```bash
cp config.example.json config.json
# Chỉnh sửa config.json bằng editor yêu thích
nano config.json
```

### Cấu hình cơ bản

| Trường | Kiểu | Ý nghĩa | Mặc định | Bắt buộc |
|---|---|---|---|---|
| `access_token` | string | OA Access Token (cách 1) | "" | Cách 1 |
| `app_id` | string | App ID OA (cách 2) | "" | Cách 2 |
| `secret_key` | string | Secret Key OA (cách 2) | "" | Cách 2 |
| `tokens_file` | string | File lưu token OAuth | `tokens.json` | Không |
| `callback_port` | int | Port callback OAuth | 18080 | Không |
| `poll_interval_sec` | int | Chu kỳ poll (giây) | 10 | Không |
| `page_size` | int | Số user mỗi trang (≤50) | 20 | Không |
| `log_file` | string | File log tin nhắn | `zaloid.log` | Không |

### Cấu hình OCR (mới)

| Trường | Kiểu | Ý nghĩa | Mặc định |
|---|---|---|---|
| `ocr.enabled` | bool | Bật/tắt OCR | `true` |
| `ocr.endpoint` | string | Ollama API endpoint | `http://localhost:11434/api/generate` |
| `ocr.model` | string | Tên model Ollama | `qwen2.5vl:7b` |
| `ocr.timeout_sec` | int | Timeout request (giây) | 120 |
| `ocr.temperature` | float | Temperature (độ sáng tạo) | 0.1 |
| `ocr.default_profile` | string | Profile phân tích mặc định | `document` |
| `ocr.download_images` | bool | Tự động tải ảnh từ URL | `true` |
| `ocr.max_image_size_mb` | int | Kích thước ảnh tối đa (MB) | 10 |

**Profiles phân tích hỗ trợ:**

| Profile | Mô tả | Ứng dụng |
|---|---|---|
| `document` | Văn bản hành chính, công văn, quyết định, thông báo | Trích xuất số ký hiệu, người ký, ngày, mật độ, nội dung |
| `invoice` | Hóa đơn, biên lai, phiếu thu | Người bán/mua, mã số thuế, mặt hàng, số tiền, thuế |
| `report` | Báo cáo, biểu đồ, bảng số liệu | Chỉ tiêu, xu hướng, kết luận, số liệu chính xác |
| `general` | Ảnh chung, meme, screenshot | Mô tả chung, trích xuất toàn bộ chữ |

### Ví dụ config.json đầy đủ

```json
{
  "access_token": "",
  "app_id": "1234567890",
  "secret_key": "your_secret_key_here",
  "tokens_file": "tokens.json",
  "callback_port": 18080,
  "poll_interval_sec": 10,
  "page_size": 20,
  "log_file": "zaloid.log",
  "ocr": {
    "enabled": true,
    "endpoint": "http://localhost:11434/api/generate",
    "model": "qwen2.5vl:7b",
    "timeout_sec": 120,
    "temperature": 0.1,
    "default_profile": "document",
    "download_images": true,
    "max_image_size_mb": 10
  }
}
```

### Biến môi trường (ưu tiên hơn file config)

```bash
export ZALO_ACCESS_TOKEN="token_oa_cua_ban"
export ZALO_APP_ID="1234567890"
export ZALO_SECRET_KEY="secret_key_cua_ban"
export MARKITDOWN_OUTPUT="$HOME/AeroMD/Markdown"  # Nếu dùng chung với AeroMD
```

---

## Tự động hóa & Triển khai Production

### Chạy nền với systemd (Linux)

Tạo file `/etc/systemd/system/zaloid.service`:

```ini
[Unit]
Description=Zalo OA Message Poller with OCR
After=network.target ollama.service
Requires=ollama.service

[Service]
Type=simple
User=zaloid
WorkingDirectory=/opt/zaloid
ExecStart=/opt/zaloid/build/zaloid /opt/zaloid/config.json
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal
Environment=ZALO_ACCESS_TOKEN=your_token_here

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now zaloid
sudo journalctl -u zaloid -f  # Xem log realtime
```

### Chạy nền với launchd (macOS)

Tạo `~/Library/LaunchAgents/com.zaloid.plist`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key><string>com.zaloid</string>
    <key>ProgramArguments</key>
    <array>
        <string>/Users/you/zaloid/zaloid/build/zaloid</string>
        <string>/Users/you/zaloid/zaloid/config.json</string>
    </array>
    <key>RunAtLoad</key><true/>
    <key>KeepAlive</key><true/>
    <key>StandardOutPath</key><string>/Users/you/zaloid/zaloid.log</string>
    <key>StandardErrorPath</key><string>/Users/you/zaloid/zaloid.error.log</string>
    <key>EnvironmentVariables</key>
    <dict>
        <key>ZALO_ACCESS_TOKEN</key><string>your_token_here</string>
    </dict>
</dict>
</plist>
```

```bash
launchctl load ~/Library/LaunchAgents/com.zaloid.plist
launchctl start com.zaloid
```

### Docker Compose (all-in-one)

Tạo `docker-compose.yml`:

```yaml
version: '3.8'

services:
  ollama:
    image: ollama/ollama:latest
    container_name: zaloid-ollama
    ports:
      - "11434:11434"
    volumes:
      - ollama_data:/root/.ollama
    restart: unless-stopped
    command: serve

  zaloid:
    build: .
    container_name: zaloid-app
    depends_on:
      - ollama
    environment:
      - ZALO_ACCESS_TOKEN=${ZALO_ACCESS_TOKEN}
      - OLLAMA_HOST=http://ollama:11434
    volumes:
      - ./config.json:/app/config.json
      - ./zaloid.log:/app/zaloid.log
    restart: unless-stopped

volumes:
  ollama_data:
```

```Dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake g++ libcurl4-openssl-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY zaloid/ ./zaloid/
RUN cd zaloid && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

CMD ["./zaloid/build/zaloid", "config.json"]
```

```bash
# Build và chạy
docker compose up --build -d

# Pull model vào Ollama container
docker exec zaloid-ollama ollama pull qwen2.5vl:7b
```

---

## Xử lý hàng loạt / Batch Processing

### Chạy một lần (one-shot) thay vì poll liên tục

Thêm flag `--once` (cần thêm code) hoặc dùng script wrapper:

```bash
#!/bin/bash
# run_once.sh - Chạy poll 1 lần rồi thoát
cd /path/to/zaloid
timeout 30 ./build/zaloid config.json || true
```

### Xử lý file log đã có (post-process)

Nếu đã có `zaloid.log` và muốn chạy lại OCR:

```bash
# Tạo script Python để parse log và gọi OCR lại
cat > reprocess_ocr.py << 'EOF'
import json, base64, requests, sys

OLLAMA_URL = "http://localhost:11434/api/generate"
MODEL = "qwen2.5vl:7b"

def analyze_image_url(url):
    # Download và base64
    import urllib.request
    img_data = urllib.request.urlopen(url).read()
    b64 = base64.b64encode(img_data).decode()
    
    prompt = """Bạn là chuyên viên phân tích văn bản hành chính. Trích xuất JSON:
{"summary":"","people":[],"decision_maker":"","work_content":"","security_level":"",
"document_type":"","issue_date":"","reference_number":"","keywords":[],"raw_ocr_text":""}"""
    
    resp = requests.post(OLLAMA_URL, json={
        "model": MODEL, "prompt": prompt, "images": [b64], "stream": False, "options": {"temperature": 0.1}
    }, timeout=120)
    return resp.json().get("response", "")

# Parse log, tìm URL ảnh, gọi OCR
# ...
EOF
python3 reprocess_ocr.py
```

---

## Troubleshooting / Xử lý sự cố

### 1. CMake không tìm thấy
```bash
# macOS
brew install cmake

# Linux
sudo apt install cmake  # hoặc yum/dnf

# Windows
choco install cmake
```

### 2. libcurl không tìm thấy
```bash
# Ubuntu/Debian
sudo apt install libcurl4-openssl-dev

# CentOS/RHEL
sudo yum install libcurl-devel

# macOS
brew install curl
# CMake thường tự tìm, nếu không:
cmake -S . -B build -DCURL_INCLUDE_DIR=/opt/homebrew/include -DCURL_LIBRARY=/opt/homebrew/lib/libcurl.dylib
```

### 3. Ollama connection refused
```bash
# Kiểm tra Ollama chạy
curl http://localhost:11434/api/tags

# Nếu lỗi: Khởi động Ollama
ollama serve &

# Hoặc Docker
docker run -d -p 11434:11434 -v ollama:/root/.ollama ollama/ollama
```

### 4. Model không tồn tại
```bash
ollama list                    # Xem model đã cài
ollama pull qwen2.5vl:7b       # Tải model
```

### 5. OCR timeout / chậm
- Giảm `ocr.timeout_sec` nếu mạng chậm
- Dùng model nhỏ hơn: `qwen2.5vl:3b`
- Đảm bảo có GPU: `ollama run qwen2.5vl:7b --verbose` kiểm tra "GPU" trong log

### 6. Zalo API error 401/403
- Token hết hạn → Chạy `./build/zaloid login` lại
- Kiểm tra quyền OA trên developers.zalo.me
- Access token OA ≠ Personal access token

### 7. Build lỗi nlohmann/json
```bash
# Xóa cache FetchContent và build lại
rm -rf build/_deps/nlohmann_json*
cmake -S . -B build
cmake --build build
```

---

## Kiến trúc & Mở rộng

### Cấu trúc thư mục
```
zaloid/
├── CMakeLists.txt
├── config.example.json
├── README.md
├── include/zaloid/
│   ├── config.hpp          # Cấu hình (kể cả OCR)
│   ├── http_client.hpp     # HTTP client wrapper (libcurl)
│   ├── auth.hpp            # OAuth v4 + PKCE
│   ├── zalo_api.hpp        # Zalo Open API client
│   ├── ocr_client.hpp      # OCR client (Ollama + Qwen2.5-VL)
│   └── poller.hpp          # Poller chính + OCR integration
├── src/
│   ├── main.cpp            # Entry point
│   ├── config.cpp          # Parse config.json
│   ├── http_client.cpp     # libcurl implementation
│   ├── auth.cpp            # OAuth + PKCE implementation
│   ├── zalo_api.cpp        # Zalo API calls
│   ├── poller.cpp          # Poll loop + OCR processing
│   └── ocr_client.cpp      # Ollama vision API client
└── build/                  # Build output (gitignored)
```

### Thêm profile OCR mới

1. Thêm enum trong `ocr_client.hpp`:
```cpp
enum class AnalysisProfile {
    Document, Invoice, Report, General, CustomProfile  // NEW
};
```

2. Thêm prompt trong `ocr_client.cpp` hàm `get_profile_prompt()`.

3. Cập nhật `config.json`: `"default_profile": "customprofile"`.

### Tích hợp vào ứng dụng khác

`zaloid` được thiết kế modular. Các class chính:
- `HttpClient` — HTTP wrapper, có thể dùng độc lập
- `ZaloApi` — Gọi Zalo Open API, trả về struct `User`, `Message`
- `OcrClient` — Gọi Ollama vision, trả về `DocumentAnalysis`
- `Poller` — Orchestrate poll loop + OCR

Có thể include headers và link thư viện vào project C++ khác.

---

## Lưu ý pháp lý

Chỉ dùng Zalo Open API chính thức với token của chính bạn. Không dùng công cụ này để can thiệp tài khoản người khác — vi phạm điều khoản dịch vụ của Zalo.

Mô hình Qwen2.5-VL chạy hoàn toàn offline qua Ollama — dữ liệu ảnh **KHÔNG** rời khỏi máy của bạn.

---

## Giấy phép

MIT License. Xem [LICENSE](LICENSE) (nếu có).

---

## Tài liệu tham khảo

- [Zalo Open API Documentation](https://developers.zalo.me/docs/official-account/)
- [Ollama](https://ollama.com/) — Chạy LLM/VLM offline
- [Qwen2.5-VL](https://github.com/QwenLM/Qwen2.5-VL) — Vision-Language Model
- [AeroMD](https://github.com/aerovfx/AeroMD) — Công cụ Markdown + OCR tham khảo
- [nlohmann/json](https://github.com/nlohmann/json) — JSON for Modern C++
- [libcurl](https://curl.se/libcurl/) — Client URL library

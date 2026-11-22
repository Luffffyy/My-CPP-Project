#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <iomanip>
#include <csignal>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <atomic>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

typedef SOCKET SocketType;
#define CLOSE_SOCKET closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

typedef int SocketType;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define CLOSE_SOCKET close
#endif

using namespace std;

// Server Configuration
const int DEFAULT_PORT = 8080;
const int HEADER_BUFFER_SIZE = 8192;
const size_t MAX_BODY_SIZE = 5 * 1024 * 1024; // 5 MB max body size

// Global State & Metrics
static SocketType g_serverSocket = INVALID_SOCKET;
static volatile sig_atomic_t g_running = 1;
static std::chrono::steady_clock::time_point g_serverStartTime;
static std::atomic<uint64_t> g_totalRequests{0};
static std::atomic<uint64_t> g_activeConnections{0};

// Task Model for In-Memory REST API
struct TaskItem {
    int id;
    string title;
    bool completed;
    string createdAt;
};

static std::mutex g_tasksMutex;
static int g_nextTaskId = 1;
static vector<TaskItem> g_tasks;

// Initialize demo tasks
void initDemoTasks() {
    lock_guard<mutex> lock(g_tasksMutex);
    g_tasks.push_back({g_nextTaskId++, "Deploy C++ server to production", false, "2026-11-22 14:00:00"});
    g_tasks.push_back({g_nextTaskId++, "Implement multithreaded socket pool", true, "2026-11-22 12:30:00"});
    g_tasks.push_back({g_nextTaskId++, "Build interactive REST API console", true, "2026-11-22 13:15:00"});
}

void handleSignal(int signum) {
    (void)signum;
    g_running = 0;
    if (g_serverSocket != INVALID_SOCKET) {
        CLOSE_SOCKET(g_serverSocket);
        g_serverSocket = INVALID_SOCKET;
    }
    cout << "\n[Info] Server shutting down gracefully...\n";
}

// Return formatted RFC 7231 GMT date for HTTP Date header
string getHttpDate() {
    auto now = chrono::system_clock::now();
    time_t tt = chrono::system_clock::to_time_t(now);
    tm gmt{};
#ifdef _WIN32
    gmtime_s(&gmt, &tt);
#else
    gmtime_r(&tt, &gmt);
#endif
    char buf[64];
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &gmt);
    return string(buf);
}

// Formatted ISO 8601 timestamp
string getIsoTimestamp() {
    auto now = chrono::system_clock::now();
    time_t tt = chrono::system_clock::to_time_t(now);
    tm gmt{};
#ifdef _WIN32
    gmtime_s(&gmt, &tt);
#else
    gmtime_r(&tt, &gmt);
#endif
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &gmt);
    return string(buf);
}

// Helper: Format uptime in human readable string
string getUptimeString() {
    auto now = chrono::steady_clock::now();
    auto elapsedSec = chrono::duration_cast<chrono::seconds>(now - g_serverStartTime).count();

    long hours = static_cast<long>(elapsedSec / 3600);
    long minutes = static_cast<long>((elapsedSec % 3600) / 60);
    long seconds = static_cast<long>(elapsedSec % 60);

    ostringstream oss;
    if (hours > 0) oss << hours << "h ";
    if (minutes > 0 || hours > 0) oss << minutes << "m ";
    oss << seconds << "s";
    return oss.str();
}

// URL percent-decoding
string urlDecode(const string& src) {
    string ret;
    ret.reserve(src.length());
    for (size_t i = 0; i < src.length(); ++i) {
        if (src[i] == '%' && i + 2 < src.length() && isxdigit(src[i + 1]) && isxdigit(src[i + 2])) {
            int val = 0;
            istringstream is(src.substr(i + 1, 2));
            if (is >> hex >> val) {
                ret += static_cast<char>(val);
                i += 2;
            } else {
                ret += src[i];
            }
        } else if (src[i] == '+') {
            ret += ' ';
        } else {
            ret += src[i];
        }
    }
    return ret;
}

// Escape string for valid JSON output
string escapeJsonString(const string& input) {
    ostringstream ss;
    for (char c : input) {
        switch (c) {
            case '"':  ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b";  break;
            case '\f': ss << "\\f";  break;
            case '\n': ss << "\\n";  break;
            case '\r': ss << "\\r";  break;
            case '\t': ss << "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < ' ') {
                    ss << "\\u" << hex << setw(4) << setfill('0') << static_cast<int>(c);
                } else {
                    ss << c;
                }
        }
    }
    return ss.str();
}

// Simple JSON string value extractor (key-value)
string extractJsonField(const string& json, const string& key) {
    string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == string::npos) return "";

    size_t colon = json.find(':', pos + search.length());
    if (colon == string::npos) return "";

    // Find value start
    size_t valStart = json.find_first_not_of(" \t\r\n", colon + 1);
    if (valStart == string::npos) return "";

    if (json[valStart] == '"') {
        // String value
        size_t endQuote = json.find('"', valStart + 1);
        if (endQuote == string::npos) return "";
        return json.substr(valStart + 1, endQuote - (valStart + 1));
    } else {
        // Number, boolean or null
        size_t valEnd = json.find_first_of(",}\t\r\n", valStart);
        if (valEnd == string::npos) valEnd = json.length();
        string val = json.substr(valStart, valEnd - valStart);
        // Trim whitespace
        while (!val.empty() && (val.back() == ' ' || val.back() == '\r' || val.back() == '\n')) val.pop_back();
        return val;
    }
}

// Return MIME type based on file extension
string getContentType(const string& fileName) {
    const auto endsWith = [&fileName](const string& suffix) {
        return fileName.size() >= suffix.size() &&
               fileName.compare(fileName.size() - suffix.size(), suffix.size(), suffix) == 0;
    };

    if (endsWith(".html") || endsWith(".htm"))
        return "text/html; charset=utf-8";
    if (endsWith(".css"))
        return "text/css; charset=utf-8";
    if (endsWith(".js"))
        return "application/javascript; charset=utf-8";
    if (endsWith(".json"))
        return "application/json; charset=utf-8";
    if (endsWith(".txt"))
        return "text/plain; charset=utf-8";
    if (endsWith(".jpg") || endsWith(".jpeg"))
        return "image/jpeg";
    if (endsWith(".png"))
        return "image/png";
    if (endsWith(".gif"))
        return "image/gif";
    if (endsWith(".svg"))
        return "image/svg+xml";
    if (endsWith(".ico"))
        return "image/x-icon";
    if (endsWith(".webp"))
        return "image/webp";
    if (endsWith(".woff2"))
        return "font/woff2";
    if (endsWith(".woff"))
        return "font/woff";
    if (endsWith(".ttf"))
        return "font/ttf";
    if (endsWith(".pdf"))
        return "application/pdf";
    if (endsWith(".wasm"))
        return "application/wasm";

    return "application/octet-stream";
}

// Robust send loop that ensures all bytes are transmitted
bool sendAll(SocketType socket, const char* data, size_t length) {
    size_t total = 0;
    while (total < length) {
        int sent = send(socket, data + total, static_cast<int>(length - total), 0);
        if (sent <= 0) {
            return false;
        }
        total += static_cast<size_t>(sent);
    }
    return true;
}

// Send an HTTP response
bool sendResponse(SocketType clientSocket,
                  const string& status,
                  const string& contentType,
                  const string& body,
                  bool isHead = false,
                  const vector<pair<string, string>>& extraHeaders = {}) {

    ostringstream responseStream;
    responseStream << "HTTP/1.1 " << status << "\r\n";
    responseStream << "Date: " << getHttpDate() << "\r\n";
    responseStream << "Server: CppHttpServer/1.2.0\r\n";
    responseStream << "Content-Type: " << contentType << "\r\n";
    responseStream << "Content-Length: " << body.size() << "\r\n";
    responseStream << "Access-Control-Allow-Origin: *\r\n";
    responseStream << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, HEAD, OPTIONS\r\n";
    responseStream << "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n";
    responseStream << "Connection: close\r\n";

    for (const auto& header : extraHeaders) {
        responseStream << header.first << ": " << header.second << "\r\n";
    }

    responseStream << "\r\n";

    string headerStr = responseStream.str();

    // Send headers
    if (!sendAll(clientSocket, headerStr.data(), headerStr.size())) {
        return false;
    }

    // Send body for non-HEAD requests
    if (!isHead && !body.empty()) {
        if (!sendAll(clientSocket, body.data(), body.size())) {
            return false;
        }
    }

    return true;
}

// Send standard styled HTML error page
void sendError(SocketType clientSocket,
               const string& statusCode,
               const string& title,
               const string& message,
               bool isHead = false,
               const vector<pair<string, string>>& extraHeaders = {}) {

    string body =
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "  <meta charset=\"UTF-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "  <title>" + statusCode + " - " + title + "</title>\n"
        "  <style>\n"
        "    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; "
        "           background: #0f172a; color: #f8fafc; display: flex; align-items: center; justify-content: center; "
        "           min-height: 100vh; margin: 0; padding: 20px; box-sizing: border-box; }\n"
        "    .card { background: #1e293b; border: 1px solid #334155; border-radius: 12px; padding: 40px; "
        "            max-width: 500px; text-align: center; box-shadow: 0 10px 25px rgba(0,0,0,0.5); }\n"
        "    h1 { color: #f43f5e; font-size: 2.2rem; margin-top: 0; margin-bottom: 15px; }\n"
        "    p { color: #94a3b8; font-size: 1.05rem; line-height: 1.6; margin-bottom: 25px; }\n"
        "    a { display: inline-block; background: #38bdf8; color: #0f172a; text-decoration: none; font-weight: bold; "
        "        padding: 10px 20px; border-radius: 6px; transition: background 0.2s; }\n"
        "    a:hover { background: #0ea5e9; }\n"
        "    .footer { margin-top: 25px; font-size: 0.85rem; color: #64748b; }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <div class=\"card\">\n"
        "    <h1>" + statusCode + "</h1>\n"
        "    <p>" + message + "</p>\n"
        "    <a href=\"/\">Return to Home</a>\n"
        "    <div class=\"footer\">C++ HTTP Server • High Performance Networking</div>\n"
        "  </div>\n"
        "</body>\n"
        "</html>\n";

    sendResponse(clientSocket, statusCode + " " + title, "text/html; charset=utf-8", body, isHead, extraHeaders);
}

// Send JSON API Response
void sendJsonResponse(SocketType clientSocket,
                      const string& status,
                      const string& jsonBody,
                      bool isHead = false) {
    sendResponse(clientSocket, status, "application/json; charset=utf-8", jsonBody, isHead);
}

// Parsed HTTP Request Structure
struct HttpRequest {
    string method;
    string rawPath;
    string path;
    string queryString;
    string version;
    unordered_map<string, string> headers;
    string body;
    string clientIp;

    // Helper to get header by case-insensitive name
    string getHeader(const string& key) const {
        string lowerKey = key;
        transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
        auto it = headers.find(lowerKey);
        return (it != headers.end()) ? it->second : "";
    }

    // Helper to parse query parameter value
    string getQueryParam(const string& paramName) const {
        if (queryString.empty()) return "";
        string target = paramName + "=";
        size_t pos = queryString.find(target);
        if (pos == string::npos) return "";

        size_t start = pos + target.length();
        size_t end = queryString.find('&', start);
        if (end == string::npos) end = queryString.length();
        return urlDecode(queryString.substr(start, end - start));
    }
};

// -------------------------------------------------------------
// REST API Handlers
// -------------------------------------------------------------

// 1. GET /api/status - Live server status, metrics, and health
void handleApiStatus(SocketType clientSocket, const HttpRequest& req) {
    auto now = chrono::steady_clock::now();
    long uptimeSeconds = static_cast<long>(chrono::duration_cast<chrono::seconds>(now - g_serverStartTime).count());

    ostringstream json;
    json << "{\n"
         << "  \"status\": \"healthy\",\n"
         << "  \"server\": \"C++ Multithreaded HTTP Server\",\n"
         << "  \"version\": \"1.2.0\",\n"
         << "  \"timestamp\": \"" << getIsoTimestamp() << "\",\n"
         << "  \"uptime\": \"" << getUptimeString() << "\",\n"
         << "  \"uptime_seconds\": " << uptimeSeconds << ",\n"
         << "  \"total_requests_served\": " << g_totalRequests.load() << ",\n"
         << "  \"active_connections\": " << g_activeConnections.load() << ",\n"
         << "  \"client_ip\": \"" << req.clientIp << "\"\n"
         << "}\n";

    sendJsonResponse(clientSocket, "200 OK", json.str(), req.method == "HEAD");
}

// 2. GET /api/info - Server metadata and capabilities
void handleApiInfo(SocketType clientSocket, const HttpRequest& req) {
    string compiler =
#if defined(__clang__)
        "Clang " __clang_version__;
#elif defined(__GNUC__)
        "GCC " __VERSION__;
#elif defined(_MSC_VER)
        "MSVC";
#else
        "Unknown Compiler";
#endif

    string os =
#if defined(__APPLE__)
        "macOS";
#elif defined(_WIN32)
        "Windows";
#elif defined(__linux__)
        "Linux";
#else
        "POSIX Compatible";
#endif

    ostringstream json;
    json << "{\n"
         << "  \"server_name\": \"CppHttpServer\",\n"
         << "  \"version\": \"1.2.0\",\n"
         << "  \"platform\": \"" << os << "\",\n"
         << "  \"compiler\": \"" << compiler << "\",\n"
         << "  \"cpp_standard\": \"" << __cplusplus << "\",\n"
         << "  \"concurrency_model\": \"Multithreaded (POSIX / std::thread)\",\n"
         << "  \"supported_methods\": [\"GET\", \"POST\", \"PUT\", \"DELETE\", \"HEAD\", \"OPTIONS\"],\n"
         << "  \"features\": [\n"
         << "    \"REST API Engine\",\n"
         << "    \"Dynamic JSON Serializer\",\n"
         << "    \"Static File Server with 15+ MIME Types\",\n"
         << "    \"Directory Traversal Guard\",\n"
         << "    \"CORS Cross-Origin Enabled\",\n"
         << "    \"Graceful Signal Handling\",\n"
         << "    \"Environment Variable Port Override\"\n"
         << "  ]\n"
         << "}\n";

    sendJsonResponse(clientSocket, "200 OK", json.str(), req.method == "HEAD");
}

// 3. POST /api/echo - Echo back payload, headers, and metadata
void handleApiEcho(SocketType clientSocket, const HttpRequest& req) {
    ostringstream json;
    json << "{\n"
         << "  \"message\": \"Echo response from C++ server\",\n"
         << "  \"method\": \"" << req.method << "\",\n"
         << "  \"path\": \"" << req.path << "\",\n"
         << "  \"query_string\": \"" << escapeJsonString(req.queryString) << "\",\n"
         << "  \"client_ip\": \"" << req.clientIp << "\",\n"
         << "  \"timestamp\": \"" << getIsoTimestamp() << "\",\n"
         << "  \"body_length\": " << req.body.length() << ",\n"
         << "  \"body\": \"" << escapeJsonString(req.body) << "\",\n"
         << "  \"headers\": {\n";

    bool first = true;
    for (const auto& kv : req.headers) {
        if (!first) json << ",\n";
        first = false;
        json << "    \"" << escapeJsonString(kv.first) << "\": \"" << escapeJsonString(kv.second) << "\"";
    }
    json << "\n  }\n}\n";

    sendJsonResponse(clientSocket, "200 OK", json.str(), req.method == "HEAD");
}

// 4. CRUD: /api/tasks - Interactive in-memory resource API
void handleApiTasks(SocketType clientSocket, const HttpRequest& req) {
    // GET /api/tasks - Retrieve all tasks
    if (req.method == "GET" || req.method == "HEAD") {
        lock_guard<mutex> lock(g_tasksMutex);
        ostringstream json;
        json << "{\n"
             << "  \"success\": true,\n"
             << "  \"count\": " << g_tasks.size() << ",\n"
             << "  \"tasks\": [\n";

        for (size_t i = 0; i < g_tasks.size(); ++i) {
            const auto& t = g_tasks[i];
            json << "    {\n"
                 << "      \"id\": " << t.id << ",\n"
                 << "      \"title\": \"" << escapeJsonString(t.title) << "\",\n"
                 << "      \"completed\": " << (t.completed ? "true" : "false") << ",\n"
                 << "      \"created_at\": \"" << t.createdAt << "\"\n"
                 << "    }" << (i + 1 < g_tasks.size() ? "," : "") << "\n";
        }
        json << "  ]\n}\n";

        sendJsonResponse(clientSocket, "200 OK", json.str(), req.method == "HEAD");
        return;
    }

    // POST /api/tasks - Create a new task
    if (req.method == "POST") {
        string title = extractJsonField(req.body, "title");
        if (title.empty()) {
            // Check if body was plain text
            if (!req.body.empty() && req.body[0] != '{') {
                title = req.body;
            } else {
                title = "New Task #" + to_string(g_nextTaskId);
            }
        }

        TaskItem newTask;
        {
            lock_guard<mutex> lock(g_tasksMutex);
            newTask.id = g_nextTaskId++;
            newTask.title = title;
            newTask.completed = false;
            newTask.createdAt = getIsoTimestamp();
            g_tasks.push_back(newTask);
        }

        ostringstream json;
        json << "{\n"
             << "  \"success\": true,\n"
             << "  \"message\": \"Task created successfully\",\n"
             << "  \"task\": {\n"
             << "    \"id\": " << newTask.id << ",\n"
             << "    \"title\": \"" << escapeJsonString(newTask.title) << "\",\n"
             << "    \"completed\": false,\n"
             << "    \"created_at\": \"" << newTask.createdAt << "\"\n"
             << "  }\n"
             << "}\n";

        sendJsonResponse(clientSocket, "201 Created", json.str());
        return;
    }

    // PUT /api/tasks - Toggle task completion status
    if (req.method == "PUT") {
        string idStr = req.getQueryParam("id");
        if (idStr.empty()) idStr = extractJsonField(req.body, "id");

        int targetId = 0;
        try { targetId = stoi(idStr); } catch (...) {}

        if (targetId <= 0) {
            sendJsonResponse(clientSocket, "400 Bad Request", "{\"success\": false, \"error\": \"Valid task ID is required via ?id=X\"}\n");
            return;
        }

        bool found = false;
        bool newState = false;
        {
            lock_guard<mutex> lock(g_tasksMutex);
            for (auto& t : g_tasks) {
                if (t.id == targetId) {
                    t.completed = !t.completed;
                    newState = t.completed;
                    found = true;
                    break;
                }
            }
        }

        if (found) {
            ostringstream json;
            json << "{\n"
                 << "  \"success\": true,\n"
                 << "  \"message\": \"Task status updated\",\n"
                 << "  \"id\": " << targetId << ",\n"
                 << "  \"completed\": " << (newState ? "true" : "false") << "\n"
                 << "}\n";
            sendJsonResponse(clientSocket, "200 OK", json.str());
        } else {
            sendJsonResponse(clientSocket, "404 Not Found", "{\"success\": false, \"error\": \"Task not found\"}\n");
        }
        return;
    }

    // DELETE /api/tasks - Delete a task by ID
    if (req.method == "DELETE") {
        string idStr = req.getQueryParam("id");
        if (idStr.empty()) idStr = extractJsonField(req.body, "id");

        int targetId = 0;
        try { targetId = stoi(idStr); } catch (...) {}

        if (targetId <= 0) {
            sendJsonResponse(clientSocket, "400 Bad Request", "{\"success\": false, \"error\": \"Valid task ID is required via ?id=X\"}\n");
            return;
        }

        bool removed = false;
        {
            lock_guard<mutex> lock(g_tasksMutex);
            auto it = remove_if(g_tasks.begin(), g_tasks.end(), [targetId](const TaskItem& t) {
                return t.id == targetId;
            });
            if (it != g_tasks.end()) {
                g_tasks.erase(it, g_tasks.end());
                removed = true;
            }
        }

        if (removed) {
            ostringstream json;
            json << "{\n"
                 << "  \"success\": true,\n"
                 << "  \"message\": \"Task deleted successfully\",\n"
                 << "  \"deleted_id\": " << targetId << "\n"
                 << "}\n";
            sendJsonResponse(clientSocket, "200 OK", json.str());
        } else {
            sendJsonResponse(clientSocket, "404 Not Found", "{\"success\": false, \"error\": \"Task not found\"}\n");
        }
        return;
    }

    vector<pair<string, string>> headers = {{"Allow", "GET, POST, PUT, DELETE, HEAD, OPTIONS"}};
    sendError(clientSocket, "405", "Method Not Allowed", "Method not supported for /api/tasks.", false, headers);
}

// -------------------------------------------------------------
// Router & Client Connection Handler
// -------------------------------------------------------------

void handleClient(SocketType clientSocket, string clientIp) {
    g_activeConnections++;
    g_totalRequests++;

    char buffer[HEADER_BUFFER_SIZE];
    int totalReceived = 0;
    string rawData;

    // Read headers until \r\n\r\n is found
    size_t headerEndPos = string::npos;
    while (headerEndPos == string::npos && totalReceived < HEADER_BUFFER_SIZE - 1) {
        int bytes = recv(clientSocket, buffer, HEADER_BUFFER_SIZE - 1 - totalReceived, 0);
        if (bytes <= 0) {
            break;
        }
        rawData.append(buffer, static_cast<size_t>(bytes));
        totalReceived += bytes;
        headerEndPos = rawData.find("\r\n\r\n");
    }

    if (rawData.empty()) {
        CLOSE_SOCKET(clientSocket);
        g_activeConnections--;
        return;
    }

    // Split headers and initial body chunk
    string headerPart;
    string bodyPart;

    if (headerEndPos != string::npos) {
        headerPart = rawData.substr(0, headerEndPos);
        bodyPart = rawData.substr(headerEndPos + 4);
    } else {
        headerPart = rawData;
    }

    istringstream headerStream(headerPart);
    string requestLine;
    if (!getline(headerStream, requestLine)) {
        CLOSE_SOCKET(clientSocket);
        g_activeConnections--;
        return;
    }

    // Strip carriage return
    if (!requestLine.empty() && requestLine.back() == '\r') {
        requestLine.pop_back();
    }

    HttpRequest req;
    req.clientIp = clientIp;

    istringstream lineStream(requestLine);
    lineStream >> req.method >> req.rawPath >> req.version;

    if (req.method.empty() || req.rawPath.empty() || req.version.empty()) {
        sendError(clientSocket, "400", "Bad Request", "Malformed HTTP request line.");
        CLOSE_SOCKET(clientSocket);
        g_activeConnections--;
        return;
    }

    // Parse HTTP Headers
    string headerLine;
    while (getline(headerStream, headerLine)) {
        if (!headerLine.empty() && headerLine.back() == '\r') {
            headerLine.pop_back();
        }
        if (headerLine.empty()) continue;

        size_t colon = headerLine.find(':');
        if (colon != string::npos) {
            string key = headerLine.substr(0, colon);
            string val = headerLine.substr(colon + 1);
            // Trim leading space in val
            size_t valStart = val.find_first_not_of(" \t");
            if (valStart != string::npos) val = val.substr(valStart);

            // Store lowercased key for case-insensitive lookup
            string lowerKey = key;
            transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
            req.headers[lowerKey] = val;
        }
    }

    // Read full body if Content-Length specified
    string contentLengthStr = req.getHeader("content-length");
    size_t expectedContentLength = 0;
    if (!contentLengthStr.empty()) {
        try { expectedContentLength = stoul(contentLengthStr); } catch (...) {}
    }

    if (expectedContentLength > MAX_BODY_SIZE) {
        sendError(clientSocket, "413", "Payload Too Large", "Request entity is larger than server limits (5 MB max).");
        CLOSE_SOCKET(clientSocket);
        g_activeConnections--;
        return;
    }

    req.body = bodyPart;
    while (req.body.length() < expectedContentLength) {
        int bytesToRead = static_cast<int>(min(static_cast<size_t>(HEADER_BUFFER_SIZE), expectedContentLength - req.body.length()));
        int bytes = recv(clientSocket, buffer, bytesToRead, 0);
        if (bytes <= 0) break;
        req.body.append(buffer, static_cast<size_t>(bytes));
    }

    // Split path and query string
    size_t qPos = req.rawPath.find('?');
    if (qPos != string::npos) {
        req.path = req.rawPath.substr(0, qPos);
        req.queryString = req.rawPath.substr(qPos + 1);
    } else {
        req.path = req.rawPath;
        req.queryString = "";
    }

    size_t fPos = req.path.find('#');
    if (fPos != string::npos) {
        req.path = req.path.substr(0, fPos);
    }

    req.path = urlDecode(req.path);

    // Handle CORS preflight OPTIONS request
    if (req.method == "OPTIONS") {
        sendResponse(clientSocket, "204 No Content", "text/plain", "");
        CLOSE_SOCKET(clientSocket);
        g_activeConnections--;
        return;
    }

    // ---------------------------------------------------------
    // Route 1: REST API Endpoints (/api/...)
    // ---------------------------------------------------------
    if (req.path.rfind("/api/", 0) == 0) {
        cout << "[API] " << req.method << " " << req.path << " -> " << clientIp << endl;

        if (req.path == "/api/status" || req.path == "/api/health") {
            handleApiStatus(clientSocket, req);
        } else if (req.path == "/api/info") {
            handleApiInfo(clientSocket, req);
        } else if (req.path == "/api/echo") {
            handleApiEcho(clientSocket, req);
        } else if (req.path == "/api/tasks" || req.path == "/api/items") {
            handleApiTasks(clientSocket, req);
        } else {
            sendJsonResponse(clientSocket, "404 Not Found",
                "{\n  \"error\": \"Endpoint not found\",\n  \"path\": \"" + escapeJsonString(req.path) + "\",\n"
                "  \"available_endpoints\": [\"/api/status\", \"/api/info\", \"/api/echo\", \"/api/tasks\"]\n}\n");
        }

        CLOSE_SOCKET(clientSocket);
        g_activeConnections--;
        return;
    }

    // ---------------------------------------------------------
    // Route 2: Static File Serving
    // ---------------------------------------------------------

    // Only GET and HEAD supported for static files
    bool isHead = (req.method == "HEAD");
    if (req.method != "GET" && !isHead) {
        vector<pair<string, string>> headers = {{"Allow", "GET, HEAD, OPTIONS"}};
        sendError(clientSocket, "405", "Method Not Allowed", "Only GET and HEAD methods are supported for static resources.", false, headers);
        CLOSE_SOCKET(clientSocket);
        g_activeConnections--;
        return;
    }

    // Default route
    if (req.path == "/" || req.path.empty()) {
        req.path = "/index.html";
    }

    // Security check: Directory traversal & hidden files
    if (req.path.find("..") != string::npos || req.path.find("/.") != string::npos || req.path.find("\\") != string::npos) {
        cout << "[Warning] Path traversal attempt from " << clientIp << ": " << req.path << endl;
        sendError(clientSocket, "403", "Forbidden", "Access to this path is strictly forbidden.", isHead);
        CLOSE_SOCKET(clientSocket);
        g_activeConnections--;
        return;
    }

    // Restrict access to source files, git, or build artifacts
    if (req.path == "/server.cpp" || req.path == "/Makefile" || req.path == "/CMakeLists.txt" || req.path == "/Dockerfile") {
        sendError(clientSocket, "403", "Forbidden", "Access to internal project configuration files is forbidden.", isHead);
        CLOSE_SOCKET(clientSocket);
        g_activeConnections--;
        return;
    }

    string fileName = "." + req.path;

    // Open requested file in binary mode
    ifstream file(fileName, ios::binary | ios::ate);

    if (!file.is_open()) {
        // Try directory /index.html
        string dirIndex = fileName + "/index.html";
        ifstream altFile(dirIndex, ios::binary | ios::ate);
        if (altFile.is_open()) {
            fileName = dirIndex;
            file = std::move(altFile);
        } else {
            cout << "[404] Not Found: " << fileName << " (Requested by " << clientIp << ")\n";
            sendError(clientSocket, "404", "Not Found", "The requested file <code>" + req.path + "</code> was not found on this server.", isHead);
            CLOSE_SOCKET(clientSocket);
            g_activeConnections--;
            return;
        }
    }

    streamsize size = file.tellg();
    file.seekg(0, ios::beg);

    string body;
    if (size > 0) {
        body.resize(static_cast<size_t>(size));
        file.read(&body[0], size);
    }
    file.close();

    string contentType = getContentType(fileName);

    sendResponse(clientSocket, "200 OK", contentType, body, isHead);
    cout << "[200] " << req.method << " " << req.path << " (" << body.size() << " bytes) -> " << clientIp << endl;

    CLOSE_SOCKET(clientSocket);
    g_activeConnections--;
}

int main(int argc, char* argv[]) {
    g_serverStartTime = chrono::steady_clock::now();
    initDemoTasks();

    // Determine Port: check argv, then PORT env variable, then default
    int port = DEFAULT_PORT;
    if (const char* envPort = getenv("PORT")) {
        try {
            port = stoi(envPort);
        } catch (...) {
            port = DEFAULT_PORT;
        }
    }
    if (argc > 1) {
        string arg1 = argv[1];
        if (arg1 == "-p" || arg1 == "--port") {
            if (argc > 2) {
                try { port = stoi(argv[2]); } catch (...) {}
            }
        } else {
            try { port = stoi(arg1); } catch (...) {}
        }
    }

#ifndef _WIN32
    // Ignore SIGPIPE to prevent termination if client disconnects during send
    signal(SIGPIPE, SIG_IGN);
    // Graceful shutdown on SIGINT and SIGTERM
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);
#endif

#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    int wsResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsResult != 0) {
        cerr << "[Error] WSAStartup failed: " << wsResult << "\n";
        return 1;
    }
#endif

    // Create TCP socket
    g_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_serverSocket == INVALID_SOCKET) {
        cerr << "[Error] Socket creation failed.\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    // Allow socket address reuse
    int option = 1;
    if (setsockopt(g_serverSocket, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&option), sizeof(option)) == SOCKET_ERROR) {
        cerr << "[Warning] Could not set SO_REUSEADDR.\n";
    }

    // Configure server address (0.0.0.0 binds to all available interfaces)
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(static_cast<uint16_t>(port));

    // Bind socket to port
    if (::bind(g_serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
        cerr << "[Error] Bind failed on port " << port << ". Port may already be in use.\n";
        CLOSE_SOCKET(g_serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    // Start listening
    if (listen(g_serverSocket, 128) == SOCKET_ERROR) {
        cerr << "[Error] Listen failed.\n";
        CLOSE_SOCKET(g_serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    cout << "========================================================\n";
    cout << "       🚀 C++ MULTITHREADED HTTP & REST API SERVER      \n";
    cout << "========================================================\n";
    cout << "  Port:         " << port << "\n";
    cout << "  Local URL:    http://localhost:" << port << "\n";
    cout << "  REST API:     http://localhost:" << port << "/api/status\n";
    cout << "  Endpoints:    /api/status, /api/info, /api/echo, /api/tasks\n";
    cout << "  Methods:      GET, POST, PUT, DELETE, HEAD, OPTIONS\n";
    cout << "  Press Ctrl+C to stop the server.\n";
    cout << "========================================================\n\n";

    // Client accept loop
    while (g_running) {
        sockaddr_in clientAddress{};
#ifdef _WIN32
        int clientSize = sizeof(clientAddress);
#else
        socklen_t clientSize = sizeof(clientAddress);
#endif

        SocketType clientSocket = accept(
            g_serverSocket,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientSize
        );

        if (clientSocket == INVALID_SOCKET) {
            if (!g_running) break;
            continue;
        }

        char clientIp[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &(clientAddress.sin_addr), clientIp, INET_ADDRSTRLEN);

        // Handle each client in a detached thread for concurrent request processing
        thread clientThread(handleClient, clientSocket, string(clientIp));
        clientThread.detach();
    }

    if (g_serverSocket != INVALID_SOCKET) {
        CLOSE_SOCKET(g_serverSocket);
    }

#ifdef _WIN32
    WSACleanup();
#endif

    cout << "[Done] Server stopped successfully.\n";
    return 0;
}

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>
#include <csignal>
#include <cstdlib>
#include <algorithm>
#include <cctype>

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

const int DEFAULT_PORT = 8080;
const int BUFFER_SIZE = 4096;

static SocketType g_serverSocket = INVALID_SOCKET;
static volatile sig_atomic_t g_running = 1;

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

// URL percent-decoding (e.g., %20 -> space)
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
    responseStream << "Server: CppHttpServer/1.0\r\n";
    responseStream << "Content-Type: " << contentType << "\r\n";
    responseStream << "Content-Length: " << body.size() << "\r\n";
    responseStream << "Access-Control-Allow-Origin: *\r\n";
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

// Handle a single client request
void handleClient(SocketType clientSocket, string clientIp) {
    char buffer[BUFFER_SIZE] = {0};

    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);

    if (bytesReceived <= 0) {
        CLOSE_SOCKET(clientSocket);
        return;
    }

    buffer[bytesReceived] = '\0';
    string request(buffer);

    // Extract HTTP method, path, and version
    string method;
    string rawPath;
    string version;

    stringstream requestStream(request);
    requestStream >> method >> rawPath >> version;

    if (method.empty() || rawPath.empty() || version.empty()) {
        sendError(clientSocket, "400", "Bad Request", "The HTTP request could not be understood or is malformed.");
        CLOSE_SOCKET(clientSocket);
        return;
    }

    // Support both GET and HEAD
    bool isHead = (method == "HEAD");
    if (method != "GET" && !isHead) {
        vector<pair<string, string>> headers = {{"Allow", "GET, HEAD"}};
        sendError(clientSocket, "405", "Method Not Allowed", "Only GET and HEAD methods are supported by this server.", false, headers);
        CLOSE_SOCKET(clientSocket);
        return;
    }

    // Strip query string and fragment
    size_t qPos = rawPath.find('?');
    string cleanPath = (qPos != string::npos) ? rawPath.substr(0, qPos) : rawPath;
    size_t fPos = cleanPath.find('#');
    if (fPos != string::npos) {
        cleanPath = cleanPath.substr(0, fPos);
    }

    // URL decode path
    string path = urlDecode(cleanPath);

    // Default route
    if (path == "/" || path.empty()) {
        path = "/index.html";
    }

    // Security check: Directory traversal and hidden files
    if (path.find("..") != string::npos || path.find("/.") != string::npos || path.find("\\") != string::npos) {
        cout << "[Warning] Path traversal attempt from " << clientIp << ": " << path << endl;
        sendError(clientSocket, "403", "Forbidden", "Access to this path is strictly forbidden.", isHead);
        CLOSE_SOCKET(clientSocket);
        return;
    }

    // Restrict access to source files, git, or build artifacts
    if (path == "/server.cpp" || path == "/Makefile" || path == "/CMakeLists.txt" || path == "/Dockerfile") {
        sendError(clientSocket, "403", "Forbidden", "Access to internal project configuration files is forbidden.", isHead);
        CLOSE_SOCKET(clientSocket);
        return;
    }

    string fileName = "." + path;

    // Open requested file in binary mode
    ifstream file(fileName, ios::binary | ios::ate);

    if (!file.is_open()) {
        // If file not found, try appending /index.html if it's a directory
        string dirIndex = fileName + "/index.html";
        ifstream altFile(dirIndex, ios::binary | ios::ate);
        if (altFile.is_open()) {
            fileName = dirIndex;
            file = std::move(altFile);
        } else {
            cout << "[404] Not Found: " << fileName << " (Requested by " << clientIp << ")\n";
            sendError(clientSocket, "404", "Not Found", "The requested file <code>" + path + "</code> was not found on this server.", isHead);
            CLOSE_SOCKET(clientSocket);
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
    cout << "[200] " << method << " " << path << " (" << body.size() << " bytes) -> " << clientIp << endl;

    CLOSE_SOCKET(clientSocket);
}

int main(int argc, char* argv[]) {
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
    if (listen(g_serverSocket, 64) == SOCKET_ERROR) {
        cerr << "[Error] Listen failed.\n";
        CLOSE_SOCKET(g_serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    cout << "========================================================\n";
    cout << "             🚀 C++ MULTITHREADED HTTP SERVER           \n";
    cout << "========================================================\n";
    cout << "  Port:         " << port << "\n";
    cout << "  Local URL:    http://localhost:" << port << "\n";
    cout << "  Status:       Ready and accepting connections\n";
    cout << "  Features:     GET, HEAD, Static Files, Traversal Guard\n";
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
            cerr << "[Warning] Failed to accept client connection.\n";
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

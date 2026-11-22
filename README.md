# C++ Multithreaded HTTP & REST API Server 🚀

A high-performance, multithreaded HTTP/1.1 and REST API web server written from scratch in **modern C++** using standard POSIX and Winsock TCP socket programming. It supports full RESTful CRUD operations, dynamic JSON generation, live telemetry metrics, concurrent request pooling, directory traversal security, and zero external runtime dependencies.

![C++](https://img.shields.io/badge/Language-C%2B%2B17%20%2F%20C%2B%2B20-blue.svg)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)
![Docker](https://img.shields.io/badge/Docker-Ready-2496ED.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

---

## ✨ Features

* **Multithreaded Socket Engine**: Dispatches incoming client TCP sockets to detached threads (`std::thread`), handling concurrent connections and requests without blocking.
* **Full RESTful API Engine**:
  * `GET /api/status`: Real-time server telemetry, uptime, lifetime requests served, and active connections.
  * `GET /api/info`: Server metadata, compiler, platform, and enabled features.
  * `POST /api/echo`: Echoes back payload body, headers, client IP, and query parameters.
  * `GET /api/tasks`: Retrieves in-memory task lists.
  * `POST /api/tasks`: Creates a new task with JSON payload (`201 Created`).
  * `PUT /api/tasks?id=X`: Toggles completion status of a task.
  * `DELETE /api/tasks?id=X`: Deletes a task by ID.
* **Interactive Frontend & CRUD App**: Built-in dashboard with real-time telemetry metrics polling, interactive API request console, and live Task Manager CRUD application.
* **CORS Preflight Support**: Full support for cross-origin requests (`OPTIONS` preflight, `Access-Control-Allow-Origin: *`).
* **Static File Serving**: Serves `.html`, `.css`, `.js`, `.json`, `.png`, `.jpg`, `.svg`, `.ico`, `.webp`, `.pdf`, `.wasm`, `.woff2` with accurate MIME types.
* **Security Hardened**:
  * Traversal protection against `..`, encoded variants, and backslashes.
  * Shielding against reading hidden files (`.*`) and project internals (`server.cpp`, `Makefile`, etc.).
  * Ignores `SIGPIPE` on POSIX systems to protect against client disconnection aborts.
  * Graceful shutdown on `SIGINT` (Ctrl+C) and `SIGTERM`.
* **Dynamic Cloud Configuration**: Automatically reads `PORT` environment variable or command-line arguments (e.g. `./server 3000`).
* **Zero External Dependencies**: Standard C++ Standard Library and system sockets only.

---

## 📁 Project Structure

```text
C++_HTTP_SERVER/
├── server.cpp          # Multithreaded HTTP server & REST API engine
├── index.html          # Interactive frontend, telemetry dashboard & CRUD UI
├── style.css           # Modern dark-mode styling
├── script.js           # Client-side telemetry poller & API consumer
├── Makefile            # Build automation
├── CMakeLists.txt      # Cross-platform CMake build configuration
├── Dockerfile          # Multi-stage production container build
├── docker-compose.yml  # Docker Compose service definition
├── .gitignore          # Git ignore rules
└── README.md           # Documentation & API reference
```

---

## 🛠️ Build & Run

### Prerequisites

* C++17 or newer compiler (`g++` or `clang++` or MSVC)
* Optional: `make`, `cmake`, or `docker`

### Method 1: Using Make (macOS & Linux)

```bash
# Build
make

# Run
make run
```

### Method 2: Direct Compilation

#### macOS / Linux:
```bash
clang++ -std=c++17 -O2 -pthread server.cpp -o server
./server
```

#### Windows (MinGW / GCC):
```bash
g++ -std=c++17 -O2 server.cpp -o server.exe -lws2_32
server.exe
```

### Method 3: Using CMake

```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./server
```

---

## 📡 REST API Reference

All API responses are formatted in RFC-compliant JSON with CORS enabled:

### 1. Server Telemetry & Status
* **Endpoint**: `GET /api/status`
* **Response**: `200 OK`
```json
{
  "status": "healthy",
  "server": "C++ Multithreaded HTTP Server",
  "version": "1.2.0",
  "timestamp": "2026-11-22T14:35:00Z",
  "uptime": "15m 32s",
  "uptime_seconds": 932,
  "total_requests_served": 142,
  "active_connections": 1,
  "client_ip": "127.0.0.1"
}
```

### 2. Server Metadata
* **Endpoint**: `GET /api/info`
* **Response**: `200 OK`
```json
{
  "server_name": "CppHttpServer",
  "version": "1.2.0",
  "platform": "macOS",
  "compiler": "Clang 16.0.0",
  "supported_methods": ["GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS"]
}
```

### 3. Request Echo
* **Endpoint**: `POST /api/echo`
* **Request Body**: `{"message": "Hello C++!"}`
* **Response**: `200 OK` (Echoes headers, client IP, method, and payload)

### 4. In-Memory Task CRUD
* **List Tasks**: `GET /api/tasks` $\rightarrow$ `200 OK`
* **Create Task**: `POST /api/tasks` with JSON `{"title": "Deploy on Kubernetes"}` $\rightarrow$ `201 Created`
* **Toggle Task**: `PUT /api/tasks?id=1` $\rightarrow$ `200 OK`
* **Delete Task**: `DELETE /api/tasks?id=1` $\rightarrow$ `200 OK`

---

## 🐳 Docker Deployment

The project includes an optimized multi-stage `Dockerfile` and `docker-compose.yml`.

### Using Docker Compose:
```bash
docker compose up --build -d
```

### Using Plain Docker:
```bash
docker build -t cpp-http-server .
docker run -d -p 8080:8080 cpp-http-server
```

Open `http://localhost:8080` in your web browser.

---

## 🌐 Cloud Deployment (Render / Railway / Fly.io / VPS)

Because the server automatically reads the `PORT` environment variable (`getenv("PORT")`), you can deploy it directly to cloud platforms:

1. **Docker-based Cloud (Render, Fly.io, Railway, DigitalOcean App Platform)**:
   - Connect your GitHub repository.
   - Select Docker environment.
   - The platform builds the `Dockerfile` and binds the container to its dynamic cloud port automatically.

2. **Linux VPS (Ubuntu / Debian / AlmaLinux)**:
   ```bash
   git clone <your-repo-url>
   cd C++_HTTP_SERVER
   make
   sudo PORT=80 ./server
   ```

---

## 📦 Pushing to Your GitHub Account

To push this project to your GitHub account:

```bash
git add .
git commit -m "feat: complete REST API engine with CRUD endpoints, telemetry and live dashboard"
git remote add origin https://github.com/<YOUR-USERNAME>/<YOUR-REPO-NAME>.git
git push -u origin main
```

---

## 📄 License

This project is open-source and available under the [MIT License](LICENSE).

# C++ Multithreaded HTTP Server 🚀

A high-performance, multithreaded HTTP/1.1 web server written from scratch in **modern C++** using POSIX and Winsock TCP socket programming. It supports concurrent connections, static asset serving, directory traversal security, and zero external runtime dependencies.

![C++](https://img.shields.io/badge/Language-C%2B%2B17%20%2F%20C%2B%2B20-blue.svg)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)
![Docker](https://img.shields.io/badge/Docker-Ready-2496ED.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

---

## ✨ Features

* **Multithreaded Architecture**: Dispatches incoming client connections into dedicated detached threads (`std::thread`), handling concurrent web browser requests without blocking.
* **HTTP/1.1 Protocol**: Supports both `GET` and `HEAD` methods with standard RFC-compliant headers (`Date`, `Server`, `Content-Length`, `Content-Type`, `Connection: close`).
* **Rich MIME Support**: Automatically identifies and serves `.html`, `.css`, `.js`, `.json`, `.png`, `.jpg`, `.svg`, `.ico`, `.webp`, `.pdf`, `.wasm`, `.woff2`, and more.
* **Security Hardening**:
  * Blocks directory traversal attacks (`..`, encoded variants, backslashes).
  * Prevents serving hidden files (`.*`) and internal source files (`server.cpp`, `Makefile`, etc.).
  * Ignores `SIGPIPE` on POSIX systems to prevent server crash when clients abort connections mid-stream.
  * Graceful shutdown on `SIGINT` (Ctrl+C) and `SIGTERM`.
* **Configurable Port**: Adapts port dynamically via `PORT` environment variable or command-line arguments (e.g. `./server 3000`), making it 100% cloud & Docker ready.
* **Zero External Dependencies**: Standard C++ standard library and system socket APIs only.
* **Interactive Web Console**: Comes with a built-in live API tester to inspect HTTP status codes and responses in real-time.

---

## 📁 Project Structure

```text
C++_HTTP_SERVER/
├── server.cpp          # Main multithreaded C++ HTTP server
├── index.html          # Interactive frontend showcase & API testbench
├── style.css           # Modern dark-mode stylesheet
├── script.js           # Client-side JavaScript & API tester
├── Makefile            # Standard build automation
├── CMakeLists.txt      # Cross-platform CMake configuration
├── Dockerfile          # Multi-stage production container build
├── docker-compose.yml  # Docker Compose service definition
├── .gitignore          # Git ignore rules
└── README.md           # Documentation
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

## 🐳 Docker Deployment

The project includes an optimized multi-stage `Dockerfile` and `docker-compose.yml`.

### Using Docker Compose:

```bash
docker compose up --build -d
```

### Using Plain Docker:

```bash
docker build -t cpp-http-server .
docker run -p 8080:8080 cpp-http-server
```

Open `http://localhost:8080` in your web browser.

---

## 🌐 Cloud Deployment (Render / Railway / Fly.io / VPS)

Because the server automatically reads the `PORT` environment variable (`getenv("PORT")`), you can deploy it directly to cloud platforms:

1. **Docker-based Cloud (Render, Fly.io, Railway, DigitalOcean App Platform)**:
   - Connect your GitHub repository.
   - Select Docker environment.
   - The platform will build using `Dockerfile` and supply its own `PORT` environment variable automatically.

2. **Linux VPS (Ubuntu / Debian / AlmaLinux)**:
   ```bash
   git clone <your-repo-url>
   cd C++_HTTP_SERVER
   make
   sudo PORT=80 ./server
   ```
   Or create a `systemd` service:
   ```ini
   [Unit]
   Description=C++ HTTP Server
   After=network.target

   [Service]
   Type=simple
   User=www-data
   WorkingDirectory=/var/www/cpp-server
   ExecStart=/var/www/cpp-server/server 8080
   Restart=always

   [Install]
   WantedBy=multi-user.target
   ```

---

## 📡 HTTP Endpoints & Responses

| Status Code | Description | Tested Case |
| ----------- | ----------- | ----------- |
| `200 OK` | Successfully found and served file | `GET /` or `GET /style.css` |
| `400 Bad Request` | Request line is missing or malformed | Malformed HTTP request string |
| `403 Forbidden` | Access to forbidden file or directory traversal | `GET /../secret` or `GET /server.cpp` |
| `404 Not Found` | Requested resource does not exist | `GET /nonexistent.html` |
| `405 Method Not Allowed` | Method is not `GET` or `HEAD` | `POST /` or `PUT /` |

---

## 📦 Pushing to Your GitHub Account

To push this project to your GitHub account:

1. **Initialize Git repository**:
   ```bash
   git init
   git add .
   git commit -m "feat: multithreaded C++ HTTP server with interactive frontend and Docker support"
   ```

2. **Link to your GitHub repository and push**:
   ```bash
   git branch -M main
   git remote add origin https://github.com/<YOUR-USERNAME>/<YOUR-REPO-NAME>.git
   git push -u origin main
   ```

---

## 📄 License

This project is open-source and available under the [MIT License](LICENSE).

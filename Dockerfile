# Multi-stage Dockerfile for C++ HTTP Server

# Stage 1: Build binary using GCC
FROM gcc:13-bookworm AS builder
WORKDIR /build

COPY server.cpp ./
RUN g++ -std=c++17 -Wall -Wextra -O2 -pthread server.cpp -o server

# Stage 2: Minimal runtime image
FROM debian:bookworm-slim
WORKDIR /app

# Copy binary from builder
COPY --from=builder /build/server ./server

# Copy static assets
COPY index.html style.css script.js ./

# Expose default HTTP server port
EXPOSE 8080
ENV PORT=8080

# Run server
CMD ["./server"]

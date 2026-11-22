# C++ HTTP Server Makefile

CXX ?= g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
TARGET = server
SOURCES = server.cpp

# Platform-specific adjustments
ifeq ($(OS),Windows_NT)
    LDFLAGS += -lws2_32
    RM = del /Q /F
    EXT = .exe
else
    RM = rm -rf
    EXT =
endif

TARGET_BIN = $(TARGET)$(EXT)

.PHONY: all clean run test help

all: $(TARGET_BIN)

$(TARGET_BIN): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
	@echo "Build successful: $(TARGET_BIN)"

run: $(TARGET_BIN)
	./$(TARGET_BIN)

clean:
	$(RM) $(TARGET_BIN) server server.exe *.o *.dSYM server_test output

help:
	@echo "Available commands:"
	@echo "  make         - Compile the C++ server"
	@echo "  make run     - Build and run the server locally"
	@echo "  make clean   - Remove binaries and build artifacts"

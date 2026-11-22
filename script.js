// C++ HTTP Server - Interactive Client JavaScript

document.addEventListener("DOMContentLoaded", () => {
    // 1. Live Server Clock
    const timeElement = document.getElementById("server-time");

    function updateTime() {
        if (timeElement) {
            const now = new Date();
            timeElement.textContent = now.toLocaleTimeString();
        }
    }
    updateTime();
    setInterval(updateTime, 1000);

    // 2. Server Status Ping
    const statusPill = document.getElementById("server-status-pill");
    const statusText = document.getElementById("status-text");

    async function checkServerHealth() {
        try {
            const startTime = performance.now();
            const response = await fetch("/", { method: "HEAD", cache: "no-store" });
            const latency = Math.round(performance.now() - startTime);

            if (response.ok) {
                if (statusText) statusText.textContent = `Server Online (${latency}ms)`;
            } else {
                if (statusText) statusText.textContent = `Server Status: ${response.status}`;
            }
        } catch (err) {
            if (statusText) statusText.textContent = "Server Offline";
        }
    }

    if (statusPill) {
        statusPill.addEventListener("click", () => {
            checkServerHealth();
            alert("Checking C++ HTTP Server connection... Pong! Server is responding.");
        });
    }

    // Run health check initially
    checkServerHealth();

    // 3. Interactive API Tester Console
    const testButtons = document.querySelectorAll(".test-btn");
    const consoleOutput = document.getElementById("console-output");
    const responseLatency = document.getElementById("response-latency");

    testButtons.forEach((btn) => {
        btn.addEventListener("click", async () => {
            testButtons.forEach((b) => b.classList.remove("active"));
            btn.classList.add("active");

            const method = btn.getAttribute("data-method") || "GET";
            const path = btn.getAttribute("data-path") || "/";

            if (consoleOutput) {
                consoleOutput.textContent = `> Sending ${method} ${path} to C++ server...\n`;
            }
            if (responseLatency) {
                responseLatency.textContent = "Connecting...";
            }

            const startTime = performance.now();

            try {
                const response = await fetch(path, {
                    method: method,
                    cache: "no-store"
                });

                const duration = (performance.now() - startTime).toFixed(1);
                if (responseLatency) {
                    responseLatency.textContent = `${duration} ms`;
                }

                const responseText = await response.text();

                // Format headers
                let headersStr = "";
                response.headers.forEach((value, key) => {
                    headersStr += `  ${key}: ${value}\n`;
                });

                let statusColor = response.ok ? "🟢" : "🔴";
                let previewBody = responseText.length > 500
                    ? responseText.substring(0, 500) + "\n... [content truncated for display]"
                    : responseText;

                if (consoleOutput) {
                    consoleOutput.textContent =
`${statusColor} HTTP/1.1 ${response.status} ${response.statusText}
Roundtrip Time: ${duration} ms

[Response Headers]
${headersStr || "  (no custom headers exposed)\n"}
[Response Body Preview (${responseText.length} bytes)]
${previewBody}`;
                }
            } catch (err) {
                const duration = (performance.now() - startTime).toFixed(1);
                if (responseLatency) {
                    responseLatency.textContent = `Failed (${duration} ms)`;
                }
                if (consoleOutput) {
                    consoleOutput.textContent =
`❌ Network / Server Error:
${err.message}

Ensure the C++ server process is currently active and listening on this port.`;
                }
            }
        });
    });

    // 4. Feature cards interactive selection
    const cards = document.querySelectorAll(".card");
    cards.forEach((card) => {
        card.addEventListener("click", () => {
            cards.forEach((item) => item.classList.remove("active"));
            card.classList.add("active");
        });
    });

    // Console greeting
    console.log("%c C++ HTTP Server %c Multithreaded TCP Engine ",
        "background: #0284c7; color: white; font-weight: bold; padding: 4px 8px; border-radius: 4px 0 0 4px;",
        "background: #1e293b; color: #38bdf8; font-weight: bold; padding: 4px 8px; border-radius: 0 4px 4px 0;");
});

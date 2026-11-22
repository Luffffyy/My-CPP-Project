// C++ Multithreaded HTTP & REST API Server - Client JavaScript

document.addEventListener("DOMContentLoaded", () => {
    // ---------------------------------------------------------
    // 1. Live Telemetry & Metrics Polling (/api/status)
    // ---------------------------------------------------------
    const metricHealth = document.getElementById("metric-health");
    const metricUptime = document.getElementById("metric-uptime");
    const metricRequests = document.getElementById("metric-requests");
    const metricConnections = document.getElementById("metric-connections");
    const statusText = document.getElementById("status-text");
    const statusPill = document.getElementById("server-status-pill");

    async function fetchServerTelemetry() {
        try {
            const start = performance.now();
            const res = await fetch("/api/status", { cache: "no-store" });
            const latency = Math.round(performance.now() - start);

            if (res.ok) {
                const data = await res.json();
                if (metricHealth) metricHealth.textContent = data.status === "healthy" ? "Healthy" : data.status;
                if (metricUptime) metricUptime.textContent = data.uptime || `${data.uptime_seconds}s`;
                if (metricRequests) metricRequests.textContent = Number(data.total_requests_served || 0).toLocaleString();
                if (metricConnections) metricConnections.textContent = data.active_connections;
                if (statusText) statusText.textContent = `Server Online (${latency}ms)`;
            } else {
                if (statusText) statusText.textContent = `Server HTTP ${res.status}`;
            }
        } catch (err) {
            if (metricHealth) metricHealth.textContent = "Offline";
            if (statusText) statusText.textContent = "Server Disconnected";
        }
    }

    if (statusPill) {
        statusPill.addEventListener("click", () => {
            fetchServerTelemetry();
            alert("Pinged C++ HTTP Server /api/status! Telemetry refreshed.");
        });
    }

    // Initial fetch and poll every 3 seconds
    fetchServerTelemetry();
    setInterval(fetchServerTelemetry, 3000);

    // ---------------------------------------------------------
    // 2. Interactive REST API Explorer
    // ---------------------------------------------------------
    const testButtons = document.querySelectorAll(".test-btn");
    const consoleOutput = document.getElementById("console-output");
    const consoleTitle = document.getElementById("console-endpoint-title");
    const responseLatency = document.getElementById("response-latency");

    testButtons.forEach((btn) => {
        btn.addEventListener("click", async () => {
            testButtons.forEach((b) => b.classList.remove("active"));
            btn.classList.add("active");

            const method = btn.getAttribute("data-method") || "GET";
            const path = btn.getAttribute("data-path") || "/";
            const reqBody = btn.getAttribute("data-body") || null;

            if (consoleTitle) consoleTitle.textContent = `${method} ${path}`;
            if (consoleOutput) consoleOutput.textContent = `> Sending HTTP ${method} ${path} to C++ server...\n`;
            if (responseLatency) responseLatency.textContent = "Processing...";

            const start = performance.now();

            try {
                const options = {
                    method: method,
                    headers: {},
                    cache: "no-store"
                };

                if (reqBody && (method === "POST" || method === "PUT")) {
                    options.headers["Content-Type"] = "application/json";
                    options.body = reqBody;
                }

                const response = await fetch(path, options);
                const duration = (performance.now() - start).toFixed(1);
                if (responseLatency) responseLatency.textContent = `${duration} ms`;

                const rawText = await response.text();

                // Format headers
                let headersStr = "";
                response.headers.forEach((val, key) => {
                    headersStr += `  ${key}: ${val}\n`;
                });

                // Format body if JSON
                let formattedBody = rawText;
                try {
                    const parsed = JSON.parse(rawText);
                    formattedBody = JSON.stringify(parsed, null, 2);
                } catch (e) {
                    // Plain text / HTML: truncate if too long
                    if (formattedBody.length > 500) {
                        formattedBody = formattedBody.substring(0, 500) + "\n... [truncated for display]";
                    }
                }

                const indicator = response.ok ? "🟢" : "🔴";
                if (consoleOutput) {
                    consoleOutput.textContent =
`${indicator} HTTP/1.1 ${response.status} ${response.statusText}
Roundtrip Latency: ${duration} ms

[Response Headers]
${headersStr || "  (no custom headers)\n"}
[Response Body]
${formattedBody}`;
                }
            } catch (err) {
                const duration = (performance.now() - start).toFixed(1);
                if (responseLatency) responseLatency.textContent = `Error (${duration} ms)`;
                if (consoleOutput) {
                    consoleOutput.textContent =
`❌ Network / Socket Error:
${err.message}

Ensure the C++ HTTP server process is running and accessible on this port.`;
                }
            }
        });
    });

    // ---------------------------------------------------------
    // 3. Live Task Manager CRUD Application (/api/tasks)
    // ---------------------------------------------------------
    const taskList = document.getElementById("task-list");
    const createTaskForm = document.getElementById("create-task-form");
    const taskInput = document.getElementById("task-input");
    const refreshTasksBtn = document.getElementById("refresh-tasks-btn");

    async function loadTasks() {
        if (!taskList) return;
        try {
            const res = await fetch("/api/tasks", { cache: "no-store" });
            if (!res.ok) throw new Error(`HTTP ${res.status}`);

            const data = await res.json();
            const tasks = data.tasks || [];

            if (tasks.length === 0) {
                taskList.innerHTML = '<li class="task-empty">No tasks in memory. Add one above!</li>';
                return;
            }

            taskList.innerHTML = "";
            tasks.forEach((t) => {
                const li = document.createElement("li");
                li.className = `task-item ${t.completed ? "completed" : ""}`;

                li.innerHTML = `
                    <div class="task-left" data-id="${t.id}">
                        <div class="task-checkbox">${t.completed ? "✓" : ""}</div>
                        <span class="task-title">${escapeHtml(t.title)}</span>
                    </div>
                    <div class="task-meta">
                        <span class="task-id-badge">#${t.id}</span>
                        <button class="task-delete-btn" data-id="${t.id}" title="Delete Task (DELETE /api/tasks?id=${t.id})">🗑️</button>
                    </div>
                `;

                // Toggle task on click
                const toggleArea = li.querySelector(".task-left");
                toggleArea.addEventListener("click", () => toggleTask(t.id));

                // Delete task
                const delBtn = li.querySelector(".task-delete-btn");
                delBtn.addEventListener("click", (e) => {
                    e.stopPropagation();
                    deleteTask(t.id);
                });

                taskList.appendChild(li);
            });
        } catch (err) {
            taskList.innerHTML = `<li class="task-empty" style="color: #ef4444;">Failed to load tasks from C++ server: ${err.message}</li>`;
        }
    }

    async function createTask(title) {
        try {
            const res = await fetch("/api/tasks", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ title: title })
            });
            if (res.ok) {
                await loadTasks();
                fetchServerTelemetry();
            } else {
                alert(`Error creating task: HTTP ${res.status}`);
            }
        } catch (err) {
            alert(`Network error creating task: ${err.message}`);
        }
    }

    async function toggleTask(id) {
        try {
            const res = await fetch(`/api/tasks?id=${id}`, {
                method: "PUT"
            });
            if (res.ok) {
                await loadTasks();
            }
        } catch (err) {
            console.error("Failed to toggle task:", err);
        }
    }

    async function deleteTask(id) {
        try {
            const res = await fetch(`/api/tasks?id=${id}`, {
                method: "DELETE"
            });
            if (res.ok) {
                await loadTasks();
                fetchServerTelemetry();
            }
        } catch (err) {
            alert(`Error deleting task: ${err.message}`);
        }
    }

    if (createTaskForm) {
        createTaskForm.addEventListener("submit", async (e) => {
            e.preventDefault();
            const text = (taskInput.value || "").trim();
            if (!text) return;
            taskInput.value = "";
            await createTask(text);
        });
    }

    if (refreshTasksBtn) {
        refreshTasksBtn.addEventListener("click", loadTasks);
    }

    function escapeHtml(str) {
        const div = document.createElement("div");
        div.textContent = str;
        return div.innerHTML;
    }

    // Load tasks on startup
    loadTasks();

    console.log("%c C++ HTTP & REST Server %c API Engine Online ",
        "background: #0284c7; color: white; font-weight: bold; padding: 4px 8px; border-radius: 4px 0 0 4px;",
        "background: #0f172a; color: #38bdf8; font-weight: bold; padding: 4px 8px; border-radius: 0 4px 4px 0;");
});

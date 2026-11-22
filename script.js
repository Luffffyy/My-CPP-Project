// C++ Multithreaded HTTP Server - Professional Frontend Application Script

document.addEventListener("DOMContentLoaded", () => {
    // -------------------------------------------------------------
    // 1. Tab Navigation System
    // -------------------------------------------------------------
    const tabs = document.querySelectorAll(".nav-tab");
    const tabPanels = document.querySelectorAll(".tab-panel");

    function switchTab(tabId) {
        tabs.forEach((t) => {
            const isMatch = t.getAttribute("data-tab") === tabId;
            t.classList.toggle("active", isMatch);
        });

        tabPanels.forEach((panel) => {
            const isMatch = panel.id === tabId;
            panel.classList.toggle("active", isMatch);
        });

        // Update URL hash without jumping
        if (history.replaceState) {
            history.replaceState(null, null, `#${tabId}`);
        }
    }

    tabs.forEach((tab) => {
        tab.addEventListener("click", () => {
            const targetId = tab.getAttribute("data-tab");
            switchTab(targetId);
        });
    });

    // Restore tab from URL hash if present
    if (window.location.hash) {
        const hashId = window.location.hash.replace("#", "");
        if (document.getElementById(hashId)) {
            switchTab(hashId);
        }
    }

    // -------------------------------------------------------------
    // 2. Global Toast Notification System
    // -------------------------------------------------------------
    const toastContainer = document.getElementById("toast-container");

    function showToast(message, type = "info") {
        if (!toastContainer) return;
        const toast = document.createElement("div");
        toast.className = `toast toast-${type}`;
        const icon = type === "success" ? "✅" : type === "error" ? "❌" : "⚡";
        toast.innerHTML = `<span>${icon}</span> <span>${message}</span>`;
        toastContainer.appendChild(toast);

        setTimeout(() => {
            toast.style.opacity = "0";
            toast.style.transform = "translateY(10px)";
            toast.style.transition = "all 0.3s ease";
            setTimeout(() => toast.remove(), 300);
        }, 3000);
    }

    // -------------------------------------------------------------
    // 3. Live Server Telemetry Polling (/api/status & /api/info)
    // -------------------------------------------------------------
    const headerStatusText = document.getElementById("header-status-text");
    const headerUptime = document.getElementById("header-uptime");
    const headerPort = document.getElementById("header-port");
    const serverStatusPill = document.getElementById("server-status-pill");

    const metricHealth = document.getElementById("metric-health");
    const metricRequests = document.getElementById("metric-requests");
    const metricUptime = document.getElementById("metric-uptime");
    const metricConnections = document.getElementById("metric-connections");

    const specOs = document.getElementById("spec-os");
    const specCompiler = document.getElementById("spec-compiler");
    const logStreamBox = document.getElementById("log-stream-box");

    function addActivityLog(method, path, status, latency) {
        if (!logStreamBox) return;
        const entry = document.createElement("div");
        entry.className = "log-entry";
        const now = new Date().toLocaleTimeString();
        entry.innerHTML = `
            <span class="log-time">[${now}]</span>
            <span class="log-text">HTTP <strong>${method}</strong> ${path} &rarr; <span style="color: ${status < 400 ? '#10b981' : '#f43f5e'}; font-weight: bold;">${status}</span> (${latency}ms)</span>
        `;
        logStreamBox.prepend(entry);

        // Keep last 40 entries
        if (logStreamBox.children.length > 40) {
            logStreamBox.lastElementChild.remove();
        }
    }

    async function fetchServerTelemetry() {
        const start = performance.now();
        try {
            const res = await fetch("/api/status", { cache: "no-store" });
            const latency = Math.round(performance.now() - start);

            if (res.ok) {
                const data = await res.json();
                if (headerStatusText) headerStatusText.textContent = `Server Online (${latency}ms)`;
                if (headerUptime) headerUptime.textContent = data.uptime || `${data.uptime_seconds}s`;
                if (metricHealth) metricHealth.textContent = data.status === "healthy" ? "Healthy" : data.status;
                if (metricRequests) metricRequests.textContent = Number(data.total_requests_served || 0).toLocaleString();
                if (metricUptime) metricUptime.textContent = data.uptime || `${data.uptime_seconds}s`;
                if (metricConnections) metricConnections.textContent = data.active_connections;
            } else {
                if (headerStatusText) headerStatusText.textContent = `HTTP ${res.status}`;
            }
        } catch (err) {
            if (headerStatusText) headerStatusText.textContent = "Server Disconnected";
            if (metricHealth) metricHealth.textContent = "Offline";
        }
    }

    async function fetchServerInfo() {
        try {
            const res = await fetch("/api/info", { cache: "no-store" });
            if (res.ok) {
                const data = await res.json();
                if (specOs) specOs.textContent = data.platform || "macOS / POSIX";
                if (specCompiler) specCompiler.textContent = data.compiler || "Clang / GCC";
            }
        } catch (err) {
            // ignore
        }
    }

    if (serverStatusPill) {
        serverStatusPill.addEventListener("click", () => {
            fetchServerTelemetry();
            showToast("Server socket connection re-verified!", "success");
        });
    }

    const btnPing = document.getElementById("btn-ping-server");
    if (btnPing) {
        btnPing.addEventListener("click", async () => {
            const start = performance.now();
            await fetchServerTelemetry();
            const latency = Math.round(performance.now() - start);
            showToast(`Pong! Server roundtrip latency: ${latency}ms`, "success");
            addActivityLog("GET", "/api/status", 200, latency);
        });
    }

    const btnCopyUrl = document.getElementById("btn-copy-url");
    if (btnCopyUrl) {
        btnCopyUrl.addEventListener("click", () => {
            navigator.clipboard.writeText(window.location.origin);
            showToast("Server base URL copied to clipboard!", "info");
        });
    }

    const btnClearLogs = document.getElementById("btn-clear-logs");
    if (btnClearLogs) {
        btnClearLogs.addEventListener("click", () => {
            if (logStreamBox) {
                logStreamBox.innerHTML = '<div class="log-entry system-msg"><span class="log-time">[System]</span><span class="log-text">Activity logs cleared.</span></div>';
            }
        });
    }

    // Telemetry Startup
    fetchServerTelemetry();
    fetchServerInfo();
    setInterval(fetchServerTelemetry, 3000);

    // -------------------------------------------------------------
    // 4. REST API Studio (Request Builder & Inspector)
    // -------------------------------------------------------------
    const presetBtns = document.querySelectorAll(".preset-btn");
    const reqMethodSelect = document.getElementById("req-method-select");
    const reqPathInput = document.getElementById("req-path-input");
    const reqBodyInput = document.getElementById("req-body-input");
    const bodyEditorWrap = document.getElementById("body-editor-wrap");
    const btnSendRequest = document.getElementById("btn-send-request");

    const resStatusChip = document.getElementById("res-status-chip");
    const resTimingChip = document.getElementById("res-timing-chip");
    const terminalBodyView = document.getElementById("terminal-body-view");
    const headersContentView = document.getElementById("headers-content-view");
    const toggleHeadersBtn = document.getElementById("toggle-headers-btn");
    const btnFormatJson = document.getElementById("btn-format-json");
    const btnCopyResponse = document.getElementById("btn-copy-response");

    function toggleBodyEditor() {
        const method = reqMethodSelect.value;
        const needsBody = (method === "POST" || method === "PUT");
        if (bodyEditorWrap) {
            bodyEditorWrap.style.display = needsBody ? "block" : "none";
        }
    }

    if (reqMethodSelect) {
        reqMethodSelect.addEventListener("change", toggleBodyEditor);
    }

    presetBtns.forEach((btn) => {
        btn.addEventListener("click", () => {
            presetBtns.forEach((b) => b.classList.remove("active"));
            btn.classList.add("active");

            const method = btn.getAttribute("data-method") || "GET";
            const path = btn.getAttribute("data-path") || "/api/status";
            const body = btn.getAttribute("data-body") || "";

            reqMethodSelect.value = method;
            reqPathInput.value = path;
            if (body && reqBodyInput) {
                try {
                    reqBodyInput.value = JSON.stringify(JSON.parse(body), null, 2);
                } catch (e) {
                    reqBodyInput.value = body;
                }
            } else if (reqBodyInput) {
                reqBodyInput.value = "";
            }
            toggleBodyEditor();
        });
    });

    async function executeApiRequest() {
        const method = reqMethodSelect.value;
        const path = (reqPathInput.value || "/").trim();
        const bodyContent = (reqBodyInput ? reqBodyInput.value : "").trim();

        if (terminalBodyView) terminalBodyView.textContent = `> Sending HTTP ${method} ${path} ...\n`;
        if (resTimingChip) resTimingChip.textContent = "Connecting...";

        const start = performance.now();

        try {
            const options = {
                method: method,
                headers: {},
                cache: "no-store"
            };

            if ((method === "POST" || method === "PUT") && bodyContent) {
                options.headers["Content-Type"] = "application/json";
                options.body = bodyContent;
            }

            const res = await fetch(path, options);
            const duration = (performance.now() - start).toFixed(1);

            // Update Status & Latency Badges
            if (resStatusChip) {
                resStatusChip.textContent = `${res.status} ${res.statusText || ""}`;
                resStatusChip.className = `status-chip chip-${res.status}`;
            }
            if (resTimingChip) {
                resTimingChip.textContent = `${duration} ms`;
            }

            // Extract Response Headers
            let headersText = "";
            res.headers.forEach((val, key) => {
                headersText += `${key}: ${val}\n`;
            });
            if (headersContentView) {
                headersContentView.textContent = headersText || "(no custom headers returned)";
            }

            // Body Parsing & Formatting
            const rawBody = await res.text();
            let displayBody = rawBody;
            try {
                const parsed = JSON.parse(rawBody);
                displayBody = JSON.stringify(parsed, null, 2);
            } catch (e) {
                // Keep raw text
            }

            if (terminalBodyView) {
                terminalBodyView.textContent = displayBody;
            }

            addActivityLog(method, path, res.status, Math.round(duration));
            showToast(`${method} ${path} completed (${duration}ms)`, res.ok ? "success" : "error");

            // Sync task count if tasks modified
            if (path.includes("/api/tasks")) {
                loadTasks();
            }
        } catch (err) {
            const duration = (performance.now() - start).toFixed(1);
            if (resStatusChip) {
                resStatusChip.textContent = "Network Error";
                resStatusChip.className = "status-chip chip-404";
            }
            if (resTimingChip) resTimingChip.textContent = `${duration} ms`;
            if (terminalBodyView) {
                terminalBodyView.textContent = `Network / Socket Exception:\n${err.message}\n\nPlease verify that the server is active on this port.`;
            }
            showToast(`Request failed: ${err.message}`, "error");
        }
    }

    if (btnSendRequest) {
        btnSendRequest.addEventListener("click", executeApiRequest);
    }

    if (btnFormatJson && reqBodyInput) {
        btnFormatJson.addEventListener("click", () => {
            try {
                const parsed = JSON.parse(reqBodyInput.value);
                reqBodyInput.value = JSON.stringify(parsed, null, 2);
            } catch (e) {
                showToast("Invalid JSON string in body editor", "error");
            }
        });
    }

    if (btnCopyResponse) {
        btnCopyResponse.addEventListener("click", () => {
            if (terminalBodyView) {
                navigator.clipboard.writeText(terminalBodyView.textContent);
                showToast("Response body copied to clipboard!", "info");
            }
        });
    }

    if (toggleHeadersBtn && headersContentView) {
        toggleHeadersBtn.addEventListener("click", () => {
            const isHidden = headersContentView.style.display === "none";
            headersContentView.style.display = isHidden ? "block" : "none";
            toggleHeadersBtn.textContent = isHidden ? "▼ Response Headers (HTTP/1.1)" : "▶ Response Headers (HTTP/1.1)";
        });
    }

    // Initialize initial state
    toggleBodyEditor();

    // -------------------------------------------------------------
    // 5. In-Memory Database (Task CRUD Application)
    // -------------------------------------------------------------
    const crudTaskList = document.getElementById("crud-task-list");
    const crudCreateForm = document.getElementById("crud-create-form");
    const crudTaskInput = document.getElementById("crud-task-input");
    const taskBadgeCount = document.getElementById("task-badge-count");
    const btnRefreshCrud = document.getElementById("btn-refresh-crud");
    const filterPills = document.querySelectorAll(".filter-pill");

    let activeFilter = "all";
    let cachedTasks = [];

    function renderTasks() {
        if (!crudTaskList) return;

        let filtered = cachedTasks;
        if (activeFilter === "pending") {
            filtered = cachedTasks.filter((t) => !t.completed);
        } else if (activeFilter === "completed") {
            filtered = cachedTasks.filter((t) => t.completed);
        }

        if (taskBadgeCount) {
            taskBadgeCount.textContent = cachedTasks.length;
        }

        if (filtered.length === 0) {
            crudTaskList.innerHTML = '<li class="task-empty">No tasks in this view. Add one above!</li>';
            return;
        }

        crudTaskList.innerHTML = "";
        filtered.forEach((task) => {
            const li = document.createElement("li");
            li.className = `task-item ${task.completed ? "completed" : ""}`;
            li.innerHTML = `
                <div class="task-left" data-id="${task.id}">
                    <div class="task-checkbox">${task.completed ? "✓" : ""}</div>
                    <span class="task-title">${escapeHtml(task.title)}</span>
                </div>
                <div class="task-meta">
                    <span class="task-id-badge">#${task.id}</span>
                    <button class="task-delete-btn" data-id="${task.id}" title="Delete (DELETE /api/tasks?id=${task.id})">🗑️</button>
                </div>
            `;

            // Toggle item
            const leftClick = li.querySelector(".task-left");
            leftClick.addEventListener("click", () => toggleTask(task.id));

            // Delete item
            const deleteBtn = li.querySelector(".task-delete-btn");
            deleteBtn.addEventListener("click", (e) => {
                e.stopPropagation();
                deleteTask(task.id);
            });

            crudTaskList.appendChild(li);
        });
    }

    async function loadTasks() {
        try {
            const res = await fetch("/api/tasks", { cache: "no-store" });
            if (!res.ok) throw new Error(`HTTP ${res.status}`);
            const data = await res.json();
            cachedTasks = data.tasks || [];
            renderTasks();
        } catch (err) {
            if (crudTaskList) {
                crudTaskList.innerHTML = `<li class="task-empty text-danger">Error loading tasks: ${err.message}</li>`;
            }
        }
    }

    async function createTask(title) {
        try {
            const res = await fetch("/api/tasks", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ title })
            });
            if (res.ok) {
                showToast(`Created task: "${title}"`, "success");
                await loadTasks();
                fetchServerTelemetry();
            } else {
                showToast(`Failed to create task: HTTP ${res.status}`, "error");
            }
        } catch (err) {
            showToast(`Network error: ${err.message}`, "error");
        }
    }

    async function toggleTask(id) {
        try {
            const res = await fetch(`/api/tasks?id=${id}`, { method: "PUT" });
            if (res.ok) {
                await loadTasks();
            }
        } catch (err) {
            showToast("Failed to toggle task", "error");
        }
    }

    async function deleteTask(id) {
        try {
            const res = await fetch(`/api/tasks?id=${id}`, { method: "DELETE" });
            if (res.ok) {
                showToast(`Deleted task #${id}`, "info");
                await loadTasks();
                fetchServerTelemetry();
            }
        } catch (err) {
            showToast(`Failed to delete task: ${err.message}`, "error");
        }
    }

    if (crudCreateForm) {
        crudCreateForm.addEventListener("submit", async (e) => {
            e.preventDefault();
            const val = (crudTaskInput.value || "").trim();
            if (!val) return;
            crudTaskInput.value = "";
            await createTask(val);
        });
    }

    if (btnRefreshCrud) {
        btnRefreshCrud.addEventListener("click", () => {
            loadTasks();
            showToast("Synced database with C++ memory store!", "success");
        });
    }

    filterPills.forEach((pill) => {
        pill.addEventListener("click", () => {
            filterPills.forEach((p) => p.classList.remove("active"));
            pill.classList.add("active");
            activeFilter = pill.getAttribute("data-filter") || "all";
            renderTasks();
        });
    });

    loadTasks();

    // -------------------------------------------------------------
    // 6. Benchmarks & Load Concurrency Tester
    // -------------------------------------------------------------
    const benchVolumeSelector = document.getElementById("bench-volume-selector");
    const benchConcurrencySelector = document.getElementById("bench-concurrency-selector");
    const benchEndpointSelect = document.getElementById("bench-endpoint-select");
    const btnRunBenchmark = document.getElementById("btn-run-benchmark");

    const benchStatusBadge = document.getElementById("bench-status-badge");
    const benchProgressBar = document.getElementById("bench-progress-bar");
    const benchRps = document.getElementById("bench-rps");
    const benchAvgLat = document.getElementById("bench-avg-lat");
    const benchMinMaxLat = document.getElementById("bench-minmax-lat");
    const benchSuccessRate = document.getElementById("bench-success-rate");
    const benchLogView = document.getElementById("bench-log-view");

    let selectedVolume = 10;
    let selectedConcurrency = 10;

    if (benchVolumeSelector) {
        benchVolumeSelector.querySelectorAll(".select-btn").forEach((btn) => {
            btn.addEventListener("click", () => {
                benchVolumeSelector.querySelectorAll(".select-btn").forEach((b) => b.classList.remove("active"));
                btn.classList.add("active");
                selectedVolume = parseInt(btn.getAttribute("data-volume"), 10);
            });
        });
    }

    if (benchConcurrencySelector) {
        benchConcurrencySelector.querySelectorAll(".select-btn").forEach((btn) => {
            btn.addEventListener("click", () => {
                benchConcurrencySelector.querySelectorAll(".select-btn").forEach((b) => b.classList.remove("active"));
                btn.classList.add("active");
                selectedConcurrency = parseInt(btn.getAttribute("data-concurrency"), 10);
            });
        });
    }

    async function runBenchmark() {
        if (!btnRunBenchmark) return;
        btnRunBenchmark.disabled = true;
        btnRunBenchmark.innerHTML = "<span>⏳ Benchmarking Socket Pool...</span>";

        const endpoint = benchEndpointSelect.value || "/api/status";
        const total = selectedVolume;
        const poolSize = selectedConcurrency;

        if (benchStatusBadge) benchStatusBadge.textContent = "Running...";
        if (benchProgressBar) benchProgressBar.style.width = "0%";
        if (benchLogView) benchLogView.textContent = `Fired ${total} asynchronous HTTP requests against ${endpoint} with ${poolSize} concurrent worker threads...\n`;

        const latencies = [];
        let successCount = 0;
        let completed = 0;

        const benchmarkStart = performance.now();

        // Worker queue runner
        let cursor = 0;
        async function worker() {
            while (cursor < total) {
                cursor++;
                const reqStart = performance.now();
                try {
                    const res = await fetch(endpoint, { cache: "no-store" });
                    const dur = performance.now() - reqStart;
                    latencies.push(dur);
                    if (res.ok) successCount++;
                } catch (e) {
                    latencies.push(performance.now() - reqStart);
                }
                completed++;
                const pct = Math.round((completed / total) * 100);
                if (benchProgressBar) benchProgressBar.style.width = `${pct}%`;
            }
        }

        const workers = [];
        for (let i = 0; i < poolSize; i++) {
            workers.push(worker());
        }

        await Promise.all(workers);

        const totalTimeMs = performance.now() - benchmarkStart;
        const totalTimeSec = totalTimeMs / 1000;
        const rps = (total / totalTimeSec).toFixed(1);

        const minLat = Math.min(...latencies).toFixed(1);
        const maxLat = Math.max(...latencies).toFixed(1);
        const avgLat = (latencies.reduce((a, b) => a + b, 0) / latencies.length).toFixed(1);
        const successPct = ((successCount / total) * 100).toFixed(0);

        // Update scorecards
        if (benchRps) benchRps.textContent = `${rps} req/s`;
        if (benchAvgLat) benchAvgLat.textContent = `${avgLat} ms`;
        if (benchMinMaxLat) benchMinMaxLat.textContent = `${minLat} / ${maxLat} ms`;
        if (benchSuccessRate) benchSuccessRate.textContent = `${successPct}%`;
        if (benchStatusBadge) benchStatusBadge.textContent = "Completed";

        if (benchLogView) {
            benchLogView.textContent +=
`Benchmark Completed in ${totalTimeMs.toFixed(1)} ms
Throughput: ${rps} Requests/sec
Latency Profile: min=${minLat}ms, avg=${avgLat}ms, max=${maxLat}ms
Success: ${successCount}/${total} (${successPct}%)`;
        }

        btnRunBenchmark.disabled = false;
        btnRunBenchmark.innerHTML = "<span>🚀 Execute Concurrency Test</span>";
        showToast(`Benchmark completed: ${rps} req/sec throughput!`, "success");
        fetchServerTelemetry();
    }

    if (btnRunBenchmark) {
        btnRunBenchmark.addEventListener("click", runBenchmark);
    }

    function escapeHtml(str) {
        const div = document.createElement("div");
        div.textContent = str;
        return div.innerHTML;
    }
});

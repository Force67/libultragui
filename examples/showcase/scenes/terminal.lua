-- Terminal - network operations center logic
-- Log feed rotation, stat updates, alert counting, tab switching

ugui.log("NOC terminal loaded")

-- Tab state

local tabs = { "tab_overview", "tab_processes", "tab_network", "tab_logs", "tab_alerts" }
local active_tab = "tab_overview"

function switch_tab(name)
    for _, t in ipairs(tabs) do
        if t == name then
            ugui.set(t, "background", "#00ff4112")
            ugui.set(t, "color", "#00ff41")
        else
            ugui.set(t, "background", "transparent")
            ugui.set(t, "color", "#00ff4180")
        end
    end
    active_tab = name
    ugui.log("Tab: " .. name)
end

function on_tab_overview(w)  switch_tab("tab_overview") end
function on_tab_processes(w) switch_tab("tab_processes") end
function on_tab_network(w)   switch_tab("tab_network") end
function on_tab_logs(w)      switch_tab("tab_logs") end
function on_tab_alerts(w)    switch_tab("tab_alerts") end

-- Log feed rotation
-- Simulates new log entries by shifting text down through the 12 log rows

local log_sources = { "nginx", "postgres", "worker", "redis", "sshd", "prom", "kernel", "cron", "containerd", "grafana" }

local log_messages = {
    nginx     = { "GET /api/health 200 2ms", "POST /api/data 201 18ms", "GET /api/metrics 200 8ms", "GET /api/status 200 1ms", "GET /static/app.js 200 0ms" },
    postgres  = { "checkpoint complete: 0 buffers written", "autovacuum: found 12 removable rows", "connection received: host=10.0.1.4", "statement: SELECT 1" },
    worker    = { "processed 847 jobs in 12.4s", "queue depth: 12 pending", "batch complete: 0 failures", "heartbeat ok" },
    redis     = { "background save completed", "evicted 0 keys", "connected clients: 42", "memory: 1.2GB used" },
    sshd      = { "session opened for user ops", "accepted publickey for deploy", "connection closed by 10.0.1.8" },
    prom      = { "scrape complete: 42 targets, 0 errors", "rule evaluation took 4ms", "WAL segment rotated" },
    kernel    = { "TCP: rto_min changed 200 -> 100ms", "oom_score_adj set to 0 for pid 2198", "net: eth0 link up 10Gbps" },
    cron      = { "job log-rotate completed (0)", "job backup started", "job metrics-export completed (0)" },
    containerd = { "container create: worker-7a3b", "image pull: registry.corp/api:latest", "gc: removed 3 snapshots" },
    grafana   = { "dashboard loaded: system-overview", "alert evaluation complete", "datasource health ok" },
}

local log_entry_count = 12847
local alert_count = 3

-- Shift log entries down and insert a new one at the top
function push_log_entry()
    -- Shift entries 11 -> 12, 10 -> 11, ... , 1 -> 2
    for i = 12, 2, -1 do
        local prev = i - 1
        -- We can read from prev row and write to current row
        -- Since we can't read widget values, we'll track state in Lua
    end

    -- Pick a random source and message
    local src = log_sources[math.random(1, #log_sources)]
    local msgs = log_messages[src]
    local msg = msgs[math.random(1, #msgs)]
    log_entry_count = log_entry_count + 1

    -- Generate a fake timestamp (increment seconds)
    local base_sec = 38 + (log_entry_count - 12847)
    local sec = base_sec % 60
    local min = (14 + math.floor(base_sec / 60)) % 60
    local hr = 2 + math.floor((14 * 60 + base_sec) / 3600)
    local timestamp = string.format("%02d:%02d:%02d", hr % 24, min, sec)

    -- Update top log entry (shift is visual approximation -- update multiple rows)
    ugui.set("l1_time", "text", timestamp)
    ugui.set("l1_src", "text", src)
    ugui.set("l1_msg", "text", msg)

    ugui.set("log_count", "text", log_entry_count .. " entries today")
    ugui.set("clock_value", "text", timestamp .. " UTC")
end

-- Stat simulation

function update_cpu()
    local usage = 25 + math.random(0, 20)
    ugui.set("cpu_val_usage", "text", usage .. "." .. math.random(0, 9) .. "%")
    ugui.set("cpu_bar_fill", "width", usage .. "%")

    local load1 = 1.5 + math.random() * 2
    local load5 = 1.2 + math.random() * 1.5
    local load15 = 1.0 + math.random() * 1.2
    ugui.set("cpu_val_load", "text", string.format("%.2f %.2f %.2f", load1, load5, load15))

    local temp = 55 + math.random(0, 15)
    ugui.set("cpu_val_temp", "text", temp .. "C")
end

function update_memory()
    local used = 10 + math.random(0, 8)
    local pct = math.floor((used / 32) * 100)
    ugui.set("mem_val_used", "text", string.format("%.1f / 32.0 GB", used + math.random() * 0.9))
    ugui.set("mem_bar_fill", "width", pct .. "%")

    local swap = math.random() * 0.8
    ugui.set("mem_val_swap", "text", string.format("%.1f / 8.0 GB", swap))

    local cache = 6 + math.random(0, 5)
    ugui.set("mem_val_cache", "text", string.format("%.1f GB", cache + math.random()))
end

function update_network()
    local rx = 600 + math.random(0, 400)
    local tx = 150 + math.random(0, 200)
    ugui.set("net_val_rx", "text", rx .. "." .. math.random(0, 9) .. " Mbit/s")
    ugui.set("net_val_tx", "text", tx .. "." .. math.random(0, 9) .. " Mbit/s")

    local conns = 3500 + math.random(0, 1500)
    ugui.set("net_val_conns", "text", string.format("%d,%03d", math.floor(conns / 1000), conns % 1000))

    local dropped = math.random(0, 2)
    ugui.set("net_val_dropped", "text", tostring(dropped))
    if dropped > 0 then
        ugui.set("net_val_dropped", "color", "#00ff41")
    end
end

function update_disk_iops()
    local r = 2000 + math.random(0, 3000)
    local w = 800 + math.random(0, 1000)
    ugui.set("disk_val_iops", "text", string.format("%d,%03d r / %d,%03d w",
        math.floor(r / 1000), r % 1000,
        math.floor(w / 1000), w % 1000))
end

-- Alert management

function add_alert()
    alert_count = alert_count + 1
    ugui.set("alerts_count", "text", tostring(alert_count))
    ugui.log("Alert triggered (#" .. alert_count .. ")")
end

function clear_alerts()
    alert_count = 0
    ugui.set("alerts_count", "text", "0")
    ugui.log("All alerts cleared")
end

-- Button handlers

function on_btn_clear_log(w)
    -- Reset log entries to blank
    for i = 1, 12 do
        ugui.set("l" .. i .. "_time", "text", "--:--:--")
        ugui.set("l" .. i .. "_src", "text", "---")
        ugui.set("l" .. i .. "_msg", "text", "")
    end
    ugui.set("log_count", "text", "0 entries today")
    log_entry_count = 0
    ugui.log("Log cleared")
end

-- Simulated tick
-- Call this periodically to simulate live data

function tick()
    push_log_entry()
    update_cpu()
    update_memory()
    update_network()
    update_disk_iops()

    -- Occasionally trigger an alert
    if math.random(1, 20) == 1 then
        add_alert()
    end

    -- Update uptime (increment seconds in display)
    local up_days = 142
    local up_str = string.format("%dd %02d:%02d:%02d", up_days, 7, 23, 41 + (log_entry_count - 12847))
    ugui.set("uptime_value", "text", up_str)

    -- Update process counts
    local running = 55 + math.random(0, 12)
    local sleeping = 247 - running
    ugui.set("proc_total", "text", "247 processes  ·  " .. sleeping .. " sleeping  ·  " .. running .. " running  ·  0 zombie")

    -- Update process CPU values for top processes
    ugui.set("p1_cpu", "text", string.format("%.1f", 8 + math.random() * 8))
    ugui.set("p2_cpu", "text", string.format("%.1f", 5 + math.random() * 7))
    ugui.set("p3_cpu", "text", string.format("%.1f", 1 + math.random() * 4))
    ugui.set("p4_cpu", "text", string.format("%.1f", 2 + math.random() * 6))
    ugui.set("p5_cpu", "text", string.format("%.1f", 0.5 + math.random() * 3))
    ugui.set("p6_cpu", "text", string.format("%.1f", 1 + math.random() * 3))

    -- Log rate
    local rate = 30 + math.random(0, 25)
    ugui.set("log_rate", "text", "~" .. rate .. "/sec")
end

-- Initialize

switch_tab("tab_overview")
ugui.log("NOC ready — " .. log_entry_count .. " log entries, " .. alert_count .. " alerts")

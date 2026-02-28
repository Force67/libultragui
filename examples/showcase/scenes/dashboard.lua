-- Mission Control: dashboard logic
-- Nav highlighting, stat updates per page, alert management

ugui.log("Mission Control loaded")

-- -- Navigation state --

local active_nav = "nav_overview"

local nav_items = {
    "nav_overview", "nav_deployments", "nav_services",
    "nav_incidents", "nav_logs", "nav_team", "nav_config"
}

function highlight_nav(name)
    for _, btn in ipairs(nav_items) do
        if btn == name then
            ugui.set(btn, "background", "#6366f118")
            ugui.set(btn, "color", "#e2e2ff")
            ugui.set(btn, "border-color", "#6366f140")
            ugui.set(btn, "border-width", 1)
        else
            ugui.set(btn, "background", "transparent")
            ugui.set(btn, "color", "#9898b8")
            ugui.set(btn, "border-width", 0)
        end
    end
    active_nav = name
end

-- -- Page data per section --

local pages = {
    nav_overview = {
        title    = "Overview",
        subtitle = "Last updated 2 minutes ago",
        uptime   = { value = "99.98%", badge = "HEALTHY",  badge_bg = "#22c55e18", badge_col = "#22c55e", detail = "43d 7h since last incident", detail_col = "#585878" },
        requests = { value = "12,847", badge = "+14%",     badge_bg = "#6366f118", badge_col = "#6366f1", detail = "Peak: 18,204 at 14:32 UTC" },
        errors   = { value = "0.42%",  badge = "WARNING",  badge_bg = "#f59e0b18", badge_col = "#f59e0b", detail = "Threshold: 0.50%", detail_col = "#f59e0b" },
    },
    nav_deployments = {
        title    = "Deployments",
        subtitle = "14 deployments today",
        uptime   = { value = "247",    badge = "TODAY",    badge_bg = "#6366f118", badge_col = "#6366f1", detail = "3,891 total this month" },
        requests = { value = "98.3%",  badge = "SUCCESS",  badge_bg = "#22c55e18", badge_col = "#22c55e", detail = "4 failures this week" },
        errors   = { value = "38s",    badge = "AVG",      badge_bg = "#6366f118", badge_col = "#6366f1", detail = "P95: 1m 42s" },
    },
    nav_services = {
        title    = "Services",
        subtitle = "18 services registered",
        uptime   = { value = "16",     badge = "RUNNING",  badge_bg = "#22c55e18", badge_col = "#22c55e", detail = "2 degraded, 0 down" },
        requests = { value = "4",      badge = "PENDING",  badge_bg = "#f59e0b18", badge_col = "#f59e0b", detail = "Awaiting health checks" },
        errors   = { value = "2.1 GB", badge = "MEMORY",   badge_bg = "#6366f118", badge_col = "#6366f1", detail = "of 8 GB allocated" },
    },
    nav_incidents = {
        title    = "Incidents",
        subtitle = "1 active incident",
        uptime   = { value = "1",      badge = "ACTIVE",   badge_bg = "#ef444418", badge_col = "#ef4444", detail = "worker-pool crash loop" },
        requests = { value = "23",     badge = "30 DAYS",  badge_bg = "#f59e0b18", badge_col = "#f59e0b", detail = "MTTR: 14 minutes" },
        errors   = { value = "99.7%",  badge = "SLA",      badge_bg = "#22c55e18", badge_col = "#22c55e", detail = "Target: 99.5%" },
    },
    nav_logs = {
        title    = "Logs",
        subtitle = "Streaming from 18 sources",
        uptime   = { value = "4.2M",   badge = "24H",      badge_bg = "#6366f118", badge_col = "#6366f1", detail = "148 GB ingested today" },
        requests = { value = "847",    badge = "ERRORS",   badge_bg = "#ef444418", badge_col = "#ef4444", detail = "Last hour across all services" },
        errors   = { value = "12ms",   badge = "LATENCY",  badge_bg = "#22c55e18", badge_col = "#22c55e", detail = "Query p50 response time" },
    },
    nav_team = {
        title    = "Team",
        subtitle = "8 members, 3 online now",
        uptime   = { value = "8",      badge = "MEMBERS",  badge_bg = "#6366f118", badge_col = "#6366f1", detail = "2 admins, 6 operators" },
        requests = { value = "142",    badge = "THIS WEEK",badge_bg = "#22c55e18", badge_col = "#22c55e", detail = "Actions performed" },
        errors   = { value = "3",      badge = "ON-CALL",  badge_bg = "#f59e0b18", badge_col = "#f59e0b", detail = "Next rotation in 4h" },
    },
    nav_config = {
        title    = "Configuration",
        subtitle = "Last change 6 hours ago",
        uptime   = { value = "38",     badge = "KEYS",     badge_bg = "#6366f118", badge_col = "#6366f1", detail = "5 modified this week" },
        requests = { value = "124",    badge = "ENV VARS", badge_bg = "#22c55e18", badge_col = "#22c55e", detail = "Across 4 environments" },
        errors   = { value = "17",     badge = "SECRETS",  badge_bg = "#f59e0b18", badge_col = "#f59e0b", detail = "2 rotated recently" },
    },
}

function navigate(nav_name)
    local page = pages[nav_name]
    if not page then return end

    ugui.log("Navigating to " .. page.title)

    -- Header
    ugui.set("page_title", "text", page.title)
    ugui.set("page_subtitle", "text", page.subtitle)

    -- Uptime card
    ugui.set("uptime_value", "text", page.uptime.value)
    ugui.set("uptime_badge_text", "text", page.uptime.badge)
    ugui.set("uptime_badge", "background", page.uptime.badge_bg)
    ugui.set("uptime_badge_text", "color", page.uptime.badge_col)
    ugui.set("uptime_detail", "text", page.uptime.detail)

    -- Requests card
    ugui.set("requests_value", "text", page.requests.value)
    ugui.set("requests_badge_text", "text", page.requests.badge)
    ugui.set("requests_badge", "background", page.requests.badge_bg)
    ugui.set("requests_badge_text", "color", page.requests.badge_col)
    ugui.set("requests_detail", "text", page.requests.detail)

    -- Errors card
    ugui.set("errors_value", "text", page.errors.value)
    ugui.set("errors_badge_text", "text", page.errors.badge)
    ugui.set("errors_badge", "background", page.errors.badge_bg)
    ugui.set("errors_badge_text", "color", page.errors.badge_col)
    ugui.set("errors_detail", "text", page.errors.detail)
    if page.errors.detail_col then
        ugui.set("errors_detail", "color", page.errors.detail_col)
    else
        ugui.set("errors_detail", "color", "#585878")
    end

    highlight_nav(nav_name)
end

-- -- Click handlers --

function on_nav_overview(w)    navigate("nav_overview") end
function on_nav_deployments(w) navigate("nav_deployments") end
function on_nav_services(w)    navigate("nav_services") end
function on_nav_incidents(w)   navigate("nav_incidents") end
function on_nav_logs(w)        navigate("nav_logs") end
function on_nav_team(w)        navigate("nav_team") end
function on_nav_config(w)      navigate("nav_config") end

function on_btn_deploy(w)
    ugui.log("Deploying to production...")
    ugui.set("d1_name", "text", "manual-deploy v0.0.0")
    ugui.set("d1_meta", "text", "production | just now | by operator")
    ugui.set("d1_duration", "text", "...")
end

function on_btn_refresh(w)
    ugui.log("Refreshing dashboard...")
    ugui.set("page_subtitle", "text", "Last updated just now")
end

function on_btn_ack_all(w)
    ugui.log("All alerts acknowledged")
    ugui.set("a1_msg", "text", "[acknowledged]")
    ugui.set("a1_msg", "color", "#585878")
    ugui.set("a2_msg", "text", "[acknowledged]")
    ugui.set("a2_msg", "color", "#585878")
    ugui.set("a3_msg", "text", "[acknowledged]")
    ugui.set("a3_msg", "color", "#585878")
    ugui.set("alerts_title", "text", "Active Alerts (0)")
end

function on_btn_view_all(w)
    ugui.log("Viewing all deployments")
    navigate("nav_deployments")
end

-- -- Initialize --

highlight_nav("nav_overview")

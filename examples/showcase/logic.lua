-- libultragui showcase: UI logic

ugui.log("Showcase logic loaded")

-- Track active nav for highlighting
local active_nav = "nav_dashboard"

local nav_buttons = {
    "nav_dashboard", "nav_settings", "nav_profile", "nav_analytics", "nav_plugins"
}

function highlight_nav(name)
    for _, btn in ipairs(nav_buttons) do
        if btn == name then
            ugui.set(btn, "background", "#2a2a4a")
            ugui.set(btn, "color", "#ffffff")
        else
            ugui.set(btn, "background", "transparent")
            ugui.set(btn, "color", "#a0a0c0")
        end
    end
    active_nav = name
end

-- Page data per section
local pages = {
    nav_dashboard = {
        title = "Dashboard",
        stats = {
            { label = "Active Users", value = "2,847", delta = "+12.5%" },
            { label = "Revenue",      value = "$48,290", delta = "+8.2%" },
            { label = "Projects",     value = "142",     delta = "+3 this week" },
        }
    },
    nav_settings = {
        title = "Settings",
        stats = {
            { label = "Config Items", value = "38",    delta = "5 changed" },
            { label = "Env Vars",     value = "124",   delta = "stable" },
            { label = "Secrets",      value = "17",    delta = "+2 rotated" },
        }
    },
    nav_profile = {
        title = "Profile",
        stats = {
            { label = "Commits",      value = "1,204", delta = "+47 this week" },
            { label = "PRs Merged",   value = "389",   delta = "+12 this week" },
            { label = "Reviews",      value = "567",   delta = "+8 this week" },
        }
    },
    nav_analytics = {
        title = "Analytics",
        stats = {
            { label = "Page Views",   value = "94,201", delta = "+18.3%" },
            { label = "Bounce Rate",  value = "32.1%",  delta = "-2.4%" },
            { label = "Avg Session",  value = "4m 12s", delta = "+0.8%" },
        }
    },
    nav_plugins = {
        title = "Plugins",
        stats = {
            { label = "Installed",    value = "23",   delta = "+2 new" },
            { label = "Updates",      value = "5",    delta = "available" },
            { label = "Custom",       value = "8",    delta = "3 active" },
        }
    },
}

function navigate(nav_name)
    local page = pages[nav_name]
    if not page then return end

    ugui.log("Navigating to " .. page.title)

    -- Update title
    ugui.set("page_title", "text", page.title)

    -- Update stat cards
    for i, stat in ipairs(page.stats) do
        ugui.set("stat_label_" .. i, "text", stat.label)
        ugui.set("stat_value_" .. i, "text", stat.value)
        ugui.set("stat_delta_" .. i, "text", stat.delta)
    end

    -- Show activity card only on dashboard
    ugui.set("activity_card", "visible", nav_name == "nav_dashboard")
    ugui.set("quick_actions", "visible", nav_name == "nav_dashboard")

    -- Highlight active nav
    highlight_nav(nav_name)
end

-- Click handlers
function on_nav_dashboard(w) navigate("nav_dashboard") end
function on_nav_settings(w)  navigate("nav_settings")  end
function on_nav_profile(w)   navigate("nav_profile")   end
function on_nav_analytics(w) navigate("nav_analytics")  end
function on_nav_plugins(w)   navigate("nav_plugins")    end

function on_action_btn(w)
    ugui.log("New Project clicked!")
end

function on_btn_deploy(w)   ugui.log("Deploying...")          end
function on_btn_rollback(w) ugui.log("Rolling back...")       end
function on_btn_scale(w)    ugui.log("Scaling up...")         end
function on_btn_monitor(w)  ugui.log("Opening monitoring...") end

-- Set initial state
highlight_nav("nav_dashboard")

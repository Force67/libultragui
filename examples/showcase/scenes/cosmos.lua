-- Cosmos: Space Mission Control logic

ugui.log("Cosmos mission control loaded")

-- -- Navigation --

local nav_items = {"nav_mission", "nav_telemetry", "nav_comms", "nav_crew", "nav_systems"}
local active_nav = "nav_mission"

local function set_nav(name)
    for _, item in ipairs(nav_items) do
        if item == name then
            ugui.set(item, "background", "#3b82f615")
            ugui.set(item, "color", "#e2e8f0")
        else
            ugui.set(item, "background", "transparent")
            ugui.set(item, "color", "#94a3b8")
        end
    end
    active_nav = name
end

function on_nav_mission(w)  set_nav("nav_mission")  ugui.log("Nav: Mission") end
function on_nav_telemetry(w) set_nav("nav_telemetry") ugui.log("Nav: Telemetry") end
function on_nav_comms(w)    set_nav("nav_comms")    ugui.log("Nav: Comms") end
function on_nav_crew(w)     set_nav("nav_crew")     ugui.log("Nav: Crew") end
function on_nav_systems(w)  set_nav("nav_systems")  ugui.log("Nav: Systems") end

-- -- Action buttons --

function on_btn_abort(w)
    ugui.log("ABORT sequence initiated!")
    ugui.set("mission_phase", "text", "ABORT SEQUENCE")
    ugui.set("mission_phase", "color", "#ef4444")
end

function on_btn_execute_burn(w)
    ugui.log("Orbital insertion burn executing")
    ugui.set("mission_phase", "text", "BURN IN PROGRESS")
    ugui.set("mission_phase", "color", "#f59e0b")
    ugui.set("val_thrust", "text", "95%")
end

function on_btn_hold(w)
    ugui.log("Hold commanded")
    ugui.set("mission_phase", "text", "HOLD")
    ugui.set("mission_phase", "color", "#64748b")
end

-- -- Initialize --

set_nav("nav_mission")

-- NERV panel: mode button handlers

local active_mode = "btn_normal"
local modes = {"btn_stop", "btn_slow", "btn_normal", "btn_racing"}

local function set_active(name)
    for _, m in ipairs(modes) do
        if m == name then
            ugui.set(m, "background", "#1a4a2a")
            ugui.set(m, "color", "#ccffdd")
        else
            ugui.set(m, "background", "#0a2a1a")
            ugui.set(m, "color", "#88ccaa")
        end
    end
    active_mode = name
end

function on_btn_stop(w)
    set_active("btn_stop")
    ugui.log("Mode: STOP")
end

function on_btn_slow(w)
    set_active("btn_slow")
    ugui.log("Mode: SLOW")
end

function on_btn_normal(w)
    set_active("btn_normal")
    ugui.log("Mode: NORMAL")
end

function on_btn_racing(w)
    set_active("btn_racing")
    ugui.log("Mode: RACING")
end

-- Initialize
set_active("btn_normal")

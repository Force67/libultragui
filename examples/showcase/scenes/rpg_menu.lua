-- RPG pause menu logic - tab switching + menu selection with detail panel

ugui.log("RPG menu loaded")

local active_tab = "tab_system"
local active_menu = "menu_quit"

-- Detail content per menu item
local menu_details = {
    menu_quicksave = { title = "Quicksave", options = { "Save to quickslot" } },
    menu_save      = { title = "Save Game", options = { "New Save", "Overwrite Save 1", "Overwrite Save 2" } },
    menu_load      = { title = "Load Game", options = { "Quicksave - Level 42", "Save 1 - Level 41", "Save 2 - Level 38" } },
    menu_mods      = { title = "Mods", options = { "Browse Mods", "Installed Mods", "Load Order" } },
    menu_settings  = { title = "Settings", options = { "Display", "Audio", "Gameplay" } },
    menu_controls  = { title = "Controls", options = { "Keyboard", "Gamepad", "Reset Defaults" } },
    menu_help      = { title = "Help", options = { "Tutorial Messages", "Controls Reference" } },
    menu_quit      = { title = "Main Menu", options = { "Desktop" } },
}

function select_item(name)
    active_menu = name
    local d = menu_details[name]
    if not d then return end

    ugui.set("detail_title", "text", d.title)
    for i = 1, 3 do
        local text = d.options[i] or ""
        ugui.set("detail_option_" .. i, "text", text)
    end
end

-- Tab handlers
function on_tab_quests(w)
    ugui.log("Switched to Quests tab")
    ugui.set("tab_quests", "color", "#e0d8c8")
    ugui.set("tab_stats", "color", "#c0b8a8")
    ugui.set("tab_system", "color", "#c0b8a8")
end

function on_tab_stats(w)
    ugui.log("Switched to General Stats tab")
    ugui.set("tab_quests", "color", "#c0b8a8")
    ugui.set("tab_stats", "color", "#e0d8c8")
    ugui.set("tab_system", "color", "#c0b8a8")
end

function on_tab_system(w)
    ugui.log("Switched to System tab")
    ugui.set("tab_quests", "color", "#c0b8a8")
    ugui.set("tab_stats", "color", "#c0b8a8")
    ugui.set("tab_system", "color", "#e0d8c8")
end

-- Menu item handlers
function on_menu_quicksave(w) select_item("menu_quicksave") end
function on_menu_save(w)      select_item("menu_save") end
function on_menu_load(w)      select_item("menu_load") end
function on_menu_mods(w)      select_item("menu_mods") end
function on_menu_settings(w)  select_item("menu_settings") end
function on_menu_controls(w)  select_item("menu_controls") end
function on_menu_help(w)      select_item("menu_help") end
function on_menu_quit(w)      select_item("menu_quit") end

-- Initialize with System tab active, Quit selected
on_tab_system()
select_item("menu_quit")

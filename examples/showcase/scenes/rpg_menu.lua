-- RPG journal/system menu logic
-- Tab switching with content changes, menu selection with detail panel

ugui.log("RPG menu loaded")

-- -- Tab state --
local active_tab = "tab_system"
local tabs = { "tab_quests", "tab_stats", "tab_system" }

function set_active_tab(name)
    active_tab = name
    for _, t in ipairs(tabs) do
        if t == name then
            ugui.set(t, "color", "#d8d0c0")
        else
            ugui.set(t, "color", "#9a9488")
        end
    end
end

-- -- Menu items per tab --
local tab_menus = {
    tab_system = {
        items = { "QUICKSAVE", "SAVE", "LOAD", "MODS", "SETTINGS", "CONTROLS", "HELP" },
        quit_text = "QUIT",
        details = {
            QUICKSAVE = { heading = "SAVE", title = "Quick Save", opts = { "Save to quickslot" } },
            SAVE      = { heading = "SAVE", title = "Save Game", opts = { "New Save", "Overwrite: Whiterun Exterior", "Overwrite: Dragonsreach" } },
            LOAD      = { heading = "LOAD", title = "Load Game", opts = { "Quicksave: Level 42", "Whiterun Exterior: Level 42", "Dragonsreach: Level 41", "Bleak Falls Barrow: Level 38" } },
            MODS      = { heading = "MODS", title = "Modifications", opts = { "Browse Creation Club", "Installed Mods (12)", "Load Order" } },
            SETTINGS  = { heading = "SETTINGS", title = "Settings", opts = { "Display", "Audio", "Gameplay", "Controls" } },
            CONTROLS  = { heading = "CONTROLS", title = "Control Scheme", opts = { "Keyboard & Mouse", "Gamepad", "Reset to Defaults" } },
            HELP      = { heading = "HELP", title = "Help Topics", opts = { "Tutorial Messages", "Controls Reference", "Gameplay Tips" } },
            QUIT      = { heading = "QUIT", title = "Main Menu", opts = { "Desktop" } },
        }
    },
    tab_quests = {
        items = { "MAIN QUEST", "COLLEGE", "COMPANIONS", "THIEVES", "DARK BRTHRHD", "CIVIL WAR", "DAEDRIC" },
        quit_text = "MISC",
        details = {
            ["MAIN QUEST"]  = { heading = "ACTIVE", title = "Main Questline", opts = { "The Way of the Voice", "A Blade in the Dark", "Diplomatic Immunity", "The Fallen", "Dragonslayer" } },
            COLLEGE         = { heading = "ACTIVE", title = "College of Winterhold", opts = { "Under Saarthal", "Hitting the Books", "The Staff of Magnus" } },
            COMPANIONS      = { heading = "ACTIVE", title = "The Companions", opts = { "Proving Honor", "The Silver Hand" } },
            THIEVES         = { heading = "COMPLETED", title = "Thieves Guild", opts = { "A Chance Arrangement", "Taking Care of Business", "Loud and Clear" } },
            ["DARK BRTHRHD"]= { heading = "ACTIVE", title = "Dark Brotherhood", opts = { "Innocence Lost", "With Friends Like These" } },
            ["CIVIL WAR"]   = { heading = "NOT STARTED", title = "Civil War", opts = { "Join the Stormcloaks", "Join the Imperial Legion" } },
            DAEDRIC         = { heading = "ACTIVE", title = "Daedric Quests", opts = { "The Black Star", "A Night to Remember", "The Mind of Madness" } },
            MISC            = { heading = "MISC", title = "Miscellaneous", opts = { "Visit the College of Winterhold", "Investigate the Bards College", "Find the Redguard woman" } },
        }
    },
    tab_stats = {
        items = { "COMBAT", "MAGIC", "STEALTH", "CRAFTING", "GENERAL", "CRIME", "BOUNTY" },
        quit_text = "PERKS",
        details = {
            COMBAT   = { heading = "SKILLS", title = "Combat", opts = { "One-Handed: 78", "Two-Handed: 34", "Archery: 56", "Block: 42", "Heavy Armor: 72" } },
            MAGIC    = { heading = "SKILLS", title = "Magic", opts = { "Destruction: 65", "Restoration: 48", "Conjuration: 52", "Alteration: 38", "Enchanting: 44" } },
            STEALTH  = { heading = "SKILLS", title = "Stealth", opts = { "Sneak: 60", "Lockpicking: 55", "Pickpocket: 28", "Speech: 42", "Alchemy: 36" } },
            CRAFTING = { heading = "SKILLS", title = "Crafting", opts = { "Smithing: 100", "Enchanting: 44", "Alchemy: 36" } },
            GENERAL  = { heading = "STATS", title = "General Stats", opts = { "Days Passed: 187", "Locations Discovered: 214", "Dungeons Cleared: 48", "Days as a Vampire: 0", "Skill Increases: 340" } },
            CRIME    = { heading = "STATS", title = "Crime Stats", opts = { "Total Bounty: 0", "Items Stolen: 142", "Assaults: 7", "Murders: 3", "Trespasses: 18" } },
            BOUNTY   = { heading = "BOUNTIES", title = "Active Bounties", opts = { "Whiterun: 0", "Solitude: 0", "Windhelm: 0", "Riften: 40", "Markarth: 0" } },
            PERKS    = { heading = "PERKS", title = "Available Perks", opts = { "Perk Points: 3", "Next Perk at Level 43" } },
        }
    },
}

local menu_widgets = {
    "menu_quicksave", "menu_save", "menu_load", "menu_mods",
    "menu_settings", "menu_controls", "menu_help"
}

function load_tab_content(tab_name)
    local tab = tab_menus[tab_name]
    if not tab then return end

    -- Update left menu item labels
    for i, label in ipairs(tab.items) do
        if i <= #menu_widgets then
            ugui.set(menu_widgets[i], "text", label)
        end
    end
    ugui.set("menu_quit", "text", tab.quit_text)

    -- Select first item by default
    local first_key = tab.items[1]
    if first_key and tab.details[first_key] then
        show_detail(tab_name, first_key)
    end
end

function show_detail(tab_name, item_key)
    local tab = tab_menus[tab_name]
    if not tab then return end
    local d = tab.details[item_key]
    if not d then return end

    ugui.set("detail_heading", "text", d.heading)
    ugui.set("detail_title", "text", d.title)
    for i = 1, 5 do
        ugui.set("detail_opt_" .. i, "text", d.opts[i] or "")
    end
end

-- -- Tab click handlers --
function on_tab_quests(w)
    set_active_tab("tab_quests")
    load_tab_content("tab_quests")
end

function on_tab_stats(w)
    set_active_tab("tab_stats")
    load_tab_content("tab_stats")
end

function on_tab_system(w)
    set_active_tab("tab_system")
    load_tab_content("tab_system")
end

-- -- Menu item click handlers --
-- These map the menu widget to the current tab's items
function menu_click(index)
    local tab = tab_menus[active_tab]
    if not tab then return end
    local key = tab.items[index]
    if key then
        show_detail(active_tab, key)
    end
end

function on_menu_quicksave(w) menu_click(1) end
function on_menu_save(w)      menu_click(2) end
function on_menu_load(w)      menu_click(3) end
function on_menu_mods(w)      menu_click(4) end
function on_menu_settings(w)  menu_click(5) end
function on_menu_controls(w)  menu_click(6) end
function on_menu_help(w)      menu_click(7) end

function on_menu_quit(w)
    local tab = tab_menus[active_tab]
    if not tab then return end
    local key = tab.quit_text
    if tab.details[key] then
        show_detail(active_tab, key)
    end
end

-- -- Initialize --
set_active_tab("tab_system")
load_tab_content("tab_system")
show_detail("tab_system", "QUIT")

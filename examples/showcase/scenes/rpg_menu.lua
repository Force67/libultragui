-- RPG pause menu logic

ugui.log("RPG menu loaded")

local active_menu = "menu_quests"
local menu_items = {
    "menu_quests", "menu_inventory", "menu_magic",
    "menu_map", "menu_skills", "menu_system"
}

-- Detail content per menu
local details = {
    menu_quests = {
        title = "QUESTS",
        items = {
            { name = "The Way of the Voice", desc = "Speak to the Greybeards" },
            { name = "A Blade in the Dark", desc = "Locate the dragon burial site" },
            { name = "Diplomatic Immunity", desc = "Infiltrate the embassy" },
            { name = "The Fallen", desc = "Capture a dragon in Dragonsreach" },
        }
    },
    menu_inventory = {
        title = "INVENTORY",
        items = {
            { name = "Daedric Sword", desc = "Damage: 48  Weight: 16" },
            { name = "Glass Armor", desc = "Armor: 76  Weight: 28" },
            { name = "Potion of Healing", desc = "Restore 80 Health" },
            { name = "Soul Gem (Grand)", desc = "Can hold grand souls" },
        }
    },
    menu_magic = {
        title = "MAGIC",
        items = {
            { name = "Fireball", desc = "40 damage in 15 foot area" },
            { name = "Fast Healing", desc = "Restore 75 Health" },
            { name = "Invisibility", desc = "Caster is invisible for 30 sec" },
            { name = "Conjure Dremora Lord", desc = "Summons a Dremora" },
        }
    },
    menu_map = {
        title = "MAP",
        items = {
            { name = "Whiterun", desc = "Capital of the central plains" },
            { name = "Solitude", desc = "Seat of the High King" },
            { name = "Windhelm", desc = "Ancient city of the Nords" },
            { name = "Riften", desc = "Home of the Thieves Guild" },
        }
    },
    menu_skills = {
        title = "SKILLS",
        items = {
            { name = "One-Handed", desc = "Level 78" },
            { name = "Destruction", desc = "Level 65" },
            { name = "Heavy Armor", desc = "Level 72" },
            { name = "Smithing", desc = "Level 100" },
        }
    },
    menu_system = {
        title = "SYSTEM",
        items = {
            { name = "Save", desc = "Save current progress" },
            { name = "Load", desc = "Load a previous save" },
            { name = "Settings", desc = "Configure gameplay options" },
            { name = "Quit", desc = "Return to main menu" },
        }
    },
}

function select_menu(name)
    active_menu = name

    -- Update detail content
    local d = details[name]
    if not d then return end

    ugui.set("detail_title", "text", d.title)
    for i, item in ipairs(d.items) do
        ugui.set("quest_" .. i .. "_name", "text", item.name)
        ugui.set("quest_" .. i .. "_desc", "text", item.desc)
    end
end

-- Click handlers for each menu item
function on_menu_quests(w) select_menu("menu_quests") end
function on_menu_inventory(w) select_menu("menu_inventory") end
function on_menu_magic(w) select_menu("menu_magic") end
function on_menu_map(w) select_menu("menu_map") end
function on_menu_skills(w) select_menu("menu_skills") end
function on_menu_system(w) select_menu("menu_system") end

-- Initialize
select_menu("menu_quests")

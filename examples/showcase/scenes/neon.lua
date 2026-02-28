-- Neon Arcade: menu logic
-- Menu selection cycling, character selection, score display

ugui.log("Neon Arcade loaded")

-- -- Menu state --

local menu_items = {
    "menu_start", "menu_continue", "menu_versus", "menu_rankings", "menu_options"
}

local menu_labels = {
    menu_start    = "START GAME",
    menu_continue = "CONTINUE",
    menu_versus   = "VS MODE",
    menu_rankings = "RANKINGS",
    menu_options  = "OPTIONS",
}

-- Which color each item uses: cyan or magenta
local menu_colors = {
    menu_start    = "cyan",
    menu_continue = "cyan",
    menu_versus   = "magenta",
    menu_rankings = "cyan",
    menu_options  = "cyan",
}

local selected_menu = 1

function get_accent(style)
    if style == "magenta" then return "#ff00aa" end
    return "#00f0ff"
end

function highlight_menu(index)
    for i, name in ipairs(menu_items) do
        local accent = get_accent(menu_colors[name])
        if i == index then
            ugui.set(name, "color", accent)
            ugui.set(name, "border-color", accent)
            ugui.set(name, "border-width", 2)
            ugui.set(name, "background", accent .. "12")
        else
            ugui.set(name, "color", accent .. "60")
            ugui.set(name, "border-color", accent .. "30")
            ugui.set(name, "border-width", 1)
            ugui.set(name, "background", "transparent")
        end
    end
    selected_menu = index
end

-- -- Character state --

local characters = {
    { name = "NOVA",   class = "STRIKER",      tier = "S TIER", accent = "magenta", power = 85 },
    { name = "CIPHER", class = "TECHNOMANCER", tier = "A TIER", accent = "cyan",    power = 72 },
    { name = "RAZE",   class = "BRAWLER",      tier = "A TIER", accent = "cyan",    power = 78 },
    { name = "VORTEX", class = "SUPPORT",      tier = "B TIER", accent = "magenta", power = 60 },
}

local selected_char = 1

function highlight_char(index)
    for i, ch in ipairs(characters) do
        local accent = get_accent(ch.accent)
        local slot = "char_slot_" .. i
        if i == index then
            ugui.set(slot, "border-color", accent)
            ugui.set(slot, "border-width", 2)
            ugui.set("char_name_" .. i, "color", accent)
            ugui.set("char_class_" .. i, "color", accent .. "80")
            ugui.set("char_rank_" .. i, "color", accent)
            ugui.set("power_bar_fill_" .. i, "background", accent)
        else
            ugui.set(slot, "border-color", accent .. "30")
            ugui.set(slot, "border-width", 1)
            ugui.set("char_name_" .. i, "color", accent .. "60")
            ugui.set("char_class_" .. i, "color", accent .. "40")
            ugui.set("char_rank_" .. i, "color", accent .. "50")
            ugui.set("power_bar_fill_" .. i, "background", accent .. "50")
        end
    end
    selected_char = index
    ugui.set("char_title", "text", "SELECT FIGHTER [" .. characters[index].name .. "]")
end

-- -- Score tracking --

local high_score = 9999999
local last_score = 4218350
local credits = 3

function format_score(n)
    local s = tostring(n)
    local result = ""
    local len = #s
    for i = 1, len do
        result = result .. s:sub(i, i)
        if (len - i) % 3 == 0 and i ~= len then
            result = result .. ","
        end
    end
    return result
end

function update_scores()
    ugui.set("score_value_1", "text", format_score(high_score))
    ugui.set("score_value_2", "text", format_score(last_score))
    ugui.set("credits_value", "text", string.format("%02d", credits))
end

-- -- Click handlers --

function on_menu_start(w)
    highlight_menu(1)
    if credits > 0 then
        credits = credits - 1
        last_score = math.random(1000000, 9999999)
        if last_score > high_score then
            high_score = last_score
        end
        update_scores()
        ugui.log("Game started! Score: " .. format_score(last_score))
    else
        ugui.log("No credits remaining!")
        ugui.set("credits_value", "color", "#ef4444")
    end
end

function on_menu_continue(w)
    highlight_menu(2)
    ugui.log("Continue: loading saved state...")
end

function on_menu_versus(w)
    highlight_menu(3)
    ugui.log("VS Mode: waiting for player 2...")
    ugui.set("title_sub", "text", "WAITING FOR P2")
end

function on_menu_rankings(w)
    highlight_menu(4)
    ugui.log("Rankings displayed")
end

function on_menu_options(w)
    highlight_menu(5)
    ugui.log("Options opened")
end

-- Character slot clicks cycle the selection
function on_char_slot_1(w) highlight_char(1) end
function on_char_slot_2(w) highlight_char(2) end
function on_char_slot_3(w) highlight_char(3) end
function on_char_slot_4(w) highlight_char(4) end

-- -- Initialize --

highlight_menu(1)
highlight_char(1)
update_scores()

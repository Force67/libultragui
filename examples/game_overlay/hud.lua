-- Game overlay HUD logic
-- Click handlers for ability and item buttons

local ability_names = {
    btn_ability_1 = "Arcane Blast",
    btn_ability_2 = "Frost Shield",
    btn_ability_3 = "Phase Shift",
    btn_ability_4 = "Meteor Storm",
}

local item_names = {
    btn_item_1 = "Health Potion",
    btn_item_2 = "Mana Crystal",
    btn_item_3 = "Scroll of Power",
    btn_item_4 = "Smoke Bomb",
}

function on_btn_ability_1(w)
    ugui.log("Cast: Arcane Blast!")
    ugui.set("notify_text", "text", "You cast Arcane Blast!")
    ugui.set("notify_text", "color", "#cc88ff")
end

function on_btn_ability_2(w)
    ugui.log("Cast: Frost Shield!")
    ugui.set("notify_text", "text", "Frost Shield activated!")
    ugui.set("notify_text", "color", "#88ccff")
end

function on_btn_ability_3(w)
    ugui.log("Cast: Phase Shift!")
    ugui.set("notify_text", "text", "Phase Shift — you blink forward!")
    ugui.set("notify_text", "color", "#88ffcc")
end

function on_btn_ability_4(w)
    ugui.log("Cast: Meteor Storm!")
    ugui.set("notify_text", "text", "METEOR STORM unleashed!")
    ugui.set("notify_text", "color", "#ff8888")
end

function on_btn_item_1(w)
    ugui.log("Used: Health Potion")
    ugui.set("notify_text", "text", "Health Potion consumed (+500 HP)")
    ugui.set("notify_text", "color", "#ff6666")
end

function on_btn_item_2(w)
    ugui.log("Used: Mana Crystal")
    ugui.set("notify_text", "text", "Mana Crystal absorbed (+300 MP)")
    ugui.set("notify_text", "color", "#6688ff")
end

function on_btn_item_3(w)
    ugui.log("Used: Scroll of Power")
    ugui.set("notify_text", "text", "Scroll of Power — ATK +25% for 30s")
    ugui.set("notify_text", "color", "#ffcc44")
end

function on_btn_item_4(w)
    ugui.log("Used: Smoke Bomb")
    ugui.set("notify_text", "text", "Smoke Bomb deployed — enemies confused!")
    ugui.set("notify_text", "color", "#aaaaaa")
end

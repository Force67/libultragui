-- Meridian - Developer Tools Landing Page logic

ugui.log("Meridian landing page loaded")

-- Navigation handlers

local nav_items = {"nav_features", "nav_pricing", "nav_docs", "nav_blog"}

local function highlight_nav(name)
    for _, item in ipairs(nav_items) do
        if item == name then
            ugui.set(item, "color", "#fafafa")
        else
            ugui.set(item, "color", "#a1a1aa")
        end
    end
end

function on_nav_features(w)
    highlight_nav("nav_features")
    ugui.log("Nav: Features")
end

function on_nav_pricing(w)
    highlight_nav("nav_pricing")
    ugui.log("Nav: Pricing")
end

function on_nav_docs(w)
    highlight_nav("nav_docs")
    ugui.log("Nav: Docs")
end

function on_nav_blog(w)
    highlight_nav("nav_blog")
    ugui.log("Nav: Blog")
end

function on_nav_logo(w)
    -- Reset nav highlight
    for _, item in ipairs(nav_items) do
        ugui.set(item, "color", "#a1a1aa")
    end
    ugui.log("Nav: Home")
end

-- CTA / action handlers

function on_nav_get_started(w)
    ugui.log("CTA: Get Started clicked")
end

function on_hero_start_free(w)
    ugui.log("CTA: Start Free (hero)")
end

function on_hero_view_docs(w)
    ugui.log("CTA: View Docs")
end

function on_btn_join_waitlist(w)
    ugui.log("Waitlist: Join requested")
    ugui.set("btn_join_waitlist", "text", "Joined!")
    ugui.set("btn_join_waitlist", "background", "#6366f1")
end

-- Pricing handlers

function on_btn_hobby_start(w)
    ugui.log("Pricing: Hobby plan selected")
end

function on_btn_pro_start(w)
    ugui.log("Pricing: Pro plan selected")
end

-- Final CTA handlers

function on_btn_cta_start(w)
    ugui.log("CTA: Start Free (footer)")
end

function on_btn_cta_contact(w)
    ugui.log("CTA: Talk to Sales")
end

-- Footer handlers

function on_footer_privacy(w)  ugui.log("Footer: Privacy") end
function on_footer_terms(w)    ugui.log("Footer: Terms") end
function on_footer_status(w)   ugui.log("Footer: Status") end

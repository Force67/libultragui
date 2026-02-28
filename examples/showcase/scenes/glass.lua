-- Glass: music player logic
-- Play/pause toggle, track navigation, volume, shuffle/repeat states

ugui.log("Glass player loaded")

-- -- Track data --

local tracks = {
    { name = "Midnight Frequencies", artist = "Aether Collective", dur = "4:28", dur_sec = 268 },
    { name = "Aurora Drift",         artist = "Aether Collective", dur = "3:51", dur_sec = 231 },
    { name = "Soft Collision",       artist = "Aether Collective", dur = "5:12", dur_sec = 312 },
    { name = "Velvet Static",        artist = "Aether Collective", dur = "4:03", dur_sec = 243 },
    { name = "Iridescent",           artist = "Aether Collective", dur = "3:37", dur_sec = 217 },
    { name = "Phase Bloom",          artist = "Aether Collective", dur = "4:44", dur_sec = 284 },
    { name = "Echoes in Glass",      artist = "Aether Collective", dur = "5:02", dur_sec = 302 },
    { name = "Dissolve",             artist = "Aether Collective", dur = "3:19", dur_sec = 199 },
}

local current_track = 1
local is_playing = true
local shuffle_on = false
local repeat_on = false
local volume = 68
local playback_sec = 102  -- 1:42 into the first track

-- -- Helpers --

function format_time(sec)
    local m = math.floor(sec / 60)
    local s = sec % 60
    return string.format("%d:%02d", m, s)
end

function update_now_playing()
    local t = tracks[current_track]

    -- Track info
    ugui.set("track_title", "text", t.name)
    ugui.set("track_artist", "text", t.artist)
    ugui.set("time_total", "text", t.dur)
    ugui.set("time_current", "text", format_time(playback_sec))

    -- Progress bar width
    local pct = math.floor((playback_sec / t.dur_sec) * 100)
    ugui.set("progress_fill", "width", pct .. "%")

    -- Play/Pause button label
    if is_playing then
        ugui.set("btn_play", "text", "Pause")
    else
        ugui.set("btn_play", "text", "Play")
    end
end

function update_playlist_highlight()
    for i = 1, #tracks do
        local row = "track_row_" .. i
        local name_widget = "tr" .. i .. "_name"

        if i == current_track then
            ugui.set(row, "background", "#a78bfa15")
            ugui.set(name_widget, "color", "#a78bfa")
            -- Show playing dot for current, hide number
            if i == 1 then
                ugui.set("tr1_dot", "background", "#a78bfa")
            end
        else
            ugui.set(row, "background", "transparent")
            ugui.set(name_widget, "color", "#ffffffb0")
            -- First track has a dot widget instead of number
            if i == 1 then
                ugui.set("tr1_dot", "background", "#ffffff20")
            end
        end
    end
end

function switch_track(index)
    if index < 1 then index = #tracks end
    if index > #tracks then index = 1 end

    current_track = index
    playback_sec = 0

    -- Update album art gradient based on track position for variety
    local gradients = {
        { "#1e1430", "#2a1848" },
        { "#141e30", "#182a48" },
        { "#1e1414", "#2a1818" },
        { "#141e1e", "#18302a" },
        { "#201430", "#2e1848" },
        { "#14201e", "#182e2a" },
        { "#1e1420", "#2a182e" },
        { "#141414", "#1e1e2a" },
    }
    local g = gradients[index]
    ugui.set("album_art", "background", g[1])
    ugui.set("album_art", "background-end", g[2])

    update_now_playing()
    update_playlist_highlight()
    ugui.log("Now playing: " .. tracks[index].name)
end

-- -- Controls --

function on_btn_play(w)
    is_playing = not is_playing
    update_now_playing()
    if is_playing then
        ugui.log("Playback resumed")
    else
        ugui.log("Playback paused")
    end
end

function on_btn_next(w)
    if shuffle_on then
        switch_track(math.random(1, #tracks))
    else
        switch_track(current_track + 1)
    end
end

function on_btn_prev(w)
    -- If more than 3 seconds in, restart track; otherwise go back
    if playback_sec > 3 then
        playback_sec = 0
        update_now_playing()
        ugui.log("Restarting: " .. tracks[current_track].name)
    else
        switch_track(current_track - 1)
    end
end

function on_btn_shuffle(w)
    shuffle_on = not shuffle_on
    if shuffle_on then
        ugui.set("btn_shuffle", "color", "#a78bfa")
        ugui.log("Shuffle enabled")
    else
        ugui.set("btn_shuffle", "color", "#ffffff30")
        ugui.log("Shuffle disabled")
    end
end

function on_btn_repeat(w)
    repeat_on = not repeat_on
    if repeat_on then
        ugui.set("btn_repeat", "color", "#a78bfa")
        ugui.log("Repeat enabled")
    else
        ugui.set("btn_repeat", "color", "#ffffff30")
        ugui.log("Repeat disabled")
    end
end

-- -- Playlist row clicks --

function on_track_row_1(w) switch_track(1) end
function on_track_row_2(w) switch_track(2) end
function on_track_row_3(w) switch_track(3) end
function on_track_row_4(w) switch_track(4) end
function on_track_row_5(w) switch_track(5) end
function on_track_row_6(w) switch_track(6) end
function on_track_row_7(w) switch_track(7) end
function on_track_row_8(w) switch_track(8) end

-- -- Top nav --

function on_btn_library(w)
    ugui.set("btn_library", "color", "#ffffff90")
    ugui.set("btn_library", "background", "#1a1a2a")
    ugui.set("btn_search", "color", "#ffffff60")
    ugui.set("btn_search", "background", "transparent")
    ugui.set("playlist_title", "text", "Luminance")
    ugui.set("playlist_meta", "text", "12 tracks | 48 min")
    ugui.log("Showing library")
end

function on_btn_search(w)
    ugui.set("btn_search", "color", "#ffffff90")
    ugui.set("btn_search", "background", "#1a1a2a")
    ugui.set("btn_library", "color", "#ffffff60")
    ugui.set("btn_library", "background", "transparent")
    ugui.set("playlist_title", "text", "Search Results")
    ugui.set("playlist_meta", "text", "Type to search...")
    ugui.log("Search mode")
end

function on_btn_queue(w)
    ugui.log("Queue view toggled")
end

-- -- Initialize --

update_now_playing()
update_playlist_highlight()

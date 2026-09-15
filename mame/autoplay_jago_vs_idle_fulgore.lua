-- Drive both player slots into a repeatable Jago-versus-Fulgore match.
--
-- Fulgore remains idle after character selection. This removes CPU-player
-- attacks from the experiment, allowing debugger traces and screenshots to
-- associate object changes with Jago's Endokuken contacts unambiguously.

local p1 = manager.machine.ioport.ports[":P1"].fields
local p2 = manager.machine.ioport.ports[":P2"].fields
local frame = 0

local p1_controlled = {
    "Coin 1",
    "1 Player Start",
    "P1 Down",
    "P1 Right",
    "P1 High Attack - Quick"
}
local p2_controlled = {
    "Coin 2",
    "2 Players Start",
    "P2 High Attack - Quick"
}

local function release(fields, names)
    for _, name in ipairs(names) do
        fields[name]:set_value(0)
    end
end

emu.register_frame_done(function()
    frame = frame + 1
    release(p1, p1_controlled)
    release(p2, p2_controlled)

    -- Start a two-human-player game and keep the second player idle.
    if frame >= 700 and frame < 704 then
        p1["Coin 1"]:set_value(1)
        p2["Coin 2"]:set_value(1)
    elseif frame >= 900 and frame < 904 then
        p1["1 Player Start"]:set_value(1)
    elseif frame >= 930 and frame < 934 then
        -- Add a fresh shared credit while the P1 select screen is active.
        p1["Coin 1"]:set_value(1)
        p2["Coin 2"]:set_value(1)
    elseif frame >= 960 and frame < 964 then
        p2["2 Players Start"]:set_value(1)
    elseif frame >= 1020 and frame < 1024 then
        -- Move P1 from Combo to Jago; leave P2 on the default Fulgore slot.
        p1["P1 Right"]:set_value(1)
    elseif frame >= 1080 and frame < 1084 then
        p1["P1 High Attack - Quick"]:set_value(1)
        p2["P2 High Attack - Quick"]:set_value(1)
    end

    -- Repeat a clean quarter-circle-forward plus quick punch every 180 frames.
    if frame >= 1450 then
        local phase = (frame - 1500) % 180
        if phase < 4 then
            p1["P1 Down"]:set_value(1)
        elseif phase < 8 then
            p1["P1 Down"]:set_value(1)
            p1["P1 Right"]:set_value(1)
        elseif phase < 12 then
            p1["P1 Right"]:set_value(1)
            p1["P1 High Attack - Quick"]:set_value(1)
        end
    end
end, "meltdown_autoplay_jago_vs_idle_fulgore")

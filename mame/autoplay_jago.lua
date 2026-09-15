-- Drive the menus into a repeatable Jago-versus-Riptor match.
--
-- This is analysis automation, not recovered game logic. It intentionally uses
-- only public MAME input fields and can run alongside debugger scripts that
-- need a live fight without requiring frame-perfect manual input.

local p1 = manager.machine.ioport.ports[":P1"].fields
local controlled = {
    "Coin 1",
    "1 Player Start",
    "P1 Down",
    "P1 Right",
    "P1 High Attack - Quick"
}

local frame = 0

local function press(name)
    p1[name]:set_value(1)
end

emu.register_frame_done(function()
    frame = frame + 1

    -- Release every synthetic input before applying this frame's actions.
    for _, name in ipairs(controlled) do
        p1[name]:set_value(0)
    end

    -- Insert credits, start, move once from Combo to Jago, then confirm.
    if (frame >= 700 and frame < 704) or
       (frame >= 900 and frame < 904) then
        press("Coin 1")
    elseif frame >= 950 and frame < 954 then
        press("1 Player Start")
    elseif frame >= 1020 and frame < 1024 then
        press("P1 Right")
    elseif frame >= 1080 and frame < 1084 then
        press("P1 High Attack - Quick")
    end

    -- Repeated quarter-circle-forward punches produce useful live activity.
    if frame >= 1600 then
        local phase = frame % 120
        if phase < 4 then
            press("P1 Down")
        elseif phase < 8 then
            press("P1 Down")
            press("P1 Right")
        elseif phase < 12 then
            press("P1 Right")
            press("P1 High Attack - Quick")
        end
    end
end, "meltdown_autoplay_jago")

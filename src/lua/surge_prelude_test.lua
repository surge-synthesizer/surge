-- surge = loadfile( "src/lua/surge_prelude.lua")(); loadfile("src/lua/surge_prelude_test.lua")(); print(test())

function test()

    a = surge.mod.ClockDivider:new()
    if (a.numerator ~= 1 and a.denominator ~= 1) then
        error( "Incorrect constructor of Clock a", 2 )
    end

    b = surge.mod.ClockDivider:new{numerator = 3}
    if (b.numerator ~= 3 and b.denominator ~= 1) then
        error( "Incorrect constructor of Clock b", 2 )
    end

    c = surge.mod.ClockDivider:new{numerator = 5, denominator = 2}
    if (c.numerator ~= 5 and c.denominator ~= 2) then
        error( "Incorrect constructor of Clock c", 2 )
    end

    dphase = 1.5 / 13
    iphase = 0
    phase = 0.0
    tick = {}
    tick["a"] = 0
    tick["b"] = 0
    tick["c"] = 0
    while (iphase < 3) do
        a:tick(iphase, phase)
        b:tick(iphase, phase)
        c:tick(iphase, phase)

        tick["a"] = tick["a"] + (a.newbeat and 1 or 0)
        tick["b"] = tick["b"] + (b.newbeat and 1 or 0)
        tick["c"] = tick["c"] + (c.newbeat and 1 or 0)

        phase = phase + dphase
        if (phase > 1 ) then
            phase = phase - 1
            iphase = iphase + 1
        end
    end

    if (tick["a"] ~= 3 and tick["b"] ~= 9 and tick["c"] ~= 8) then
        error( "Tick calculation off", 2)
    end

    return 1
    end


-- This document is loaded in each Surge XT session and provides a set of built-in helpers
-- we've found handy when writing modulators. Consider it as a library of functions.
-- For each official update of Surge XT we will freeze the state of the prelude as stable
-- and not break included functions after that.
--
-- If you have ideas for other useful functions that could be added here, by all means
-- contact us over GitHub or Discord and let us know!

local surge = {}
local mod = {}


-- MATH FUNCTIONS --


-- signum function returns -1 for negative numbers, 0 for zero, 1 for positive numbers
function math.sgn(x)
    return (x > 0 and 1) or (x < 0 and -1) or 0
end

-- sign function returns -1 for negative numbers and 1 for positive numbers or zero
function math.sign(x)
    return (x < 0 and -1) or 1
end

-- linearly interpolates value from in range to out range
function math.rescale(value, in_min, in_max, out_min, out_max)
    return (((value - in_min) * (out_max - out_min)) / (in_max - in_min)) + out_min
end

-- returns the norm of the two components (hypotenuse)
function math.norm(a, b)
    return math.sqrt(a ^ 2 + b ^ 2)
end

-- returns the absolute range between the two numbers
function math.range(a, b)
    return math.abs(a - b)
end

-- returns the frequency of the given MIDI note number under A440 equal temperament
-- ref is optional and allows specifying a different center frequency for
-- MIDI note 69 (middle A)
function math.note_to_freq(note, ref)
    local default = 440
    ref = ref or default
    return 2^((note - 69) / 12) * ref
end

-- returns the fractional MIDI note number matching given frequency under A440
-- equal temperament
-- ref is optional and allows specifying a different center frequency for
-- MIDI note 69 (middle A)
function math.freq_to_note(freq, ref)
    local default = 440
    ref = ref or default
    return 12 * math.log((freq / ref), 2) + 69
end

-- returns greatest common denominator between a and b
-- use with integers only!
function math.gcd(a, b)
    local x = a
    local y = b
    local t

    while y ~= 0 do
        t = y
        y = x % y
        x = t
    end

    return x
end

-- returns least common multiple between a and b
-- use with integers only!
function math.lcm(a, b)
    local t = a

    while t % b ~= 0 do
        t = t + a
    end

    return t
end


-- BUILT-IN MODULATORS --


--- Clock Divider ---


mod.ClockDivider =
{
    numerator = 1,
    denominator = 1,
    prioribeat = -1,
    newbeat = false,
    intphase = 0, -- increase from 0 up to n
    ibeat = 0,    -- wraps with denominator
    phase = 0
}

mod.ClockDivider.new = function(self, o)
    o = o or {}
    setmetatable(o, self)
    self.__index = self
    return o
end

mod.ClockDivider.tick = function(self, intphase, phase)
    local beat = (intphase + phase) * self.numerator / self.denominator
    local ibeat = math.floor(beat)

    self.intphase = ibeat
    self.ibeat = ibeat % self.numerator
    self.phase = beat - ibeat
    self.newbeat = false

    if (ibeat ~= self.prioribeat) then
        self.newbeat = true
    end

    self.prioribeat = ibeat
end


--- AHD Envelope ---


mod.AHDEnvelope =
{
    a = 0.1,
    h = 0.1,
    d = 0.7
}

mod.AHDEnvelope.new = function(self, o)
    o = o or {}
    setmetatable(o, self)
    self.__index = self
    return o
end

mod.AHDEnvelope.at = function(self, phase)
    if (phase <= 0) then
        return 0.0
    elseif (phase < self.a) then
        return phase / self.a
    elseif (phase < self.a + self.h) then
        return 1.0
    elseif (phase < self.a + self.h + self.d) then
        return 1.0 - (phase - (self.a + self.h)) / self.d
    else
        return 0.0
    end
end


--- Slew Limiter ---

mod.Slew =
{
    prior = 0,
    blockfactor = 1,
    block_size = 0,
    samplerate = 0,
    calculate = true,
    isfirst = true
}

mod.Slew.new = function(self, o)
    o = o or {}
    o.blockfactor = 0
    setmetatable(o, self)
    self.__index = self

    if (o.samplerate == 0 or o.block_size == 0) then
        o.calculate = false
    end

    o.blockfactor = (o.samplerate/48000.0)/(o.block_size/32.0)

    return o
end

mod.Slew.run = function(self, input, uprate, downrate)

    if (not self.calculate) then
        return 0
    end

    if (self.isfirst) then
        self.prior = input
        self.isfirst = false
    end

    if (uprate == nil) then
        uprate = .2
    end
    
    if (uprate < 0) then
        uprate = 0
    end
    
    uplimit = 1 /(1 + (10000 * self.blockfactor) * uprate^3)
    
    if (downrate == nil) then
        downlimit = uplimit
    else
        if (downrate < 0) then
            downrate = 0
        end
        downlimit = 1 /(1 + (10000 * self.blockfactor) * downrate^3)
    end
    
    delta = input - self.prior

    if (delta > limit) then
        delta = limit
    end

    if (delta < -limit) then
        delta = -limit
    end

    self.prior = self.prior + delta
    
    return self.prior
end


surge.mod = mod

return surge


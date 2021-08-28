-- surge_prelude
--
-- The surge prelude is loaded in each surge session and provides a set of
-- built in utilities we've found handy in writing modulators. At each release
-- point, we will snap the prelude as stable and not break apis after that.

local surge = {}
local mod = {}

mod.ClockDivider = { numerator = 1,
              denominator = 1,
              prioribeat = -1,
              newbeat = false,
              intphase = 0, -- increase from 0 up to n
              ibeat = 0, -- wraps with denominator
              phase = 0
            }
mod.ClockDivider.new = function(self,o)
    o = o or {}
    setmetatable(o, self)
    self.__index = self
    return o
end

mod.ClockDivider.tick = function(self, intphase, phase)
    beat = (intphase + phase) * self.numerator / self.denominator
    ibeat = math.floor(beat)
    self.intphase = ibeat
    self.ibeat = ibeat % self.numerator
    self.phase = beat - ibeat
    self.newbeat = false
    if (ibeat ~= self.prioribeat) then
        self.newbeat = true
    end
    self.prioribeat = ibeat
end

surge["mod"] = mod
return surge

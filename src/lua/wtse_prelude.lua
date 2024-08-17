-- This document is loaded in each Surge XT session and provides a set of built-in helpers
-- we've found handy when generating wavetables. Consider it as a library of functions.
-- For each official update of Surge XT we will freeze the state of the prelude as stable
-- and not break included functions after that.
--
-- If you have ideas for other useful functions that could be added here, by all means
-- contact us over GitHub or Discord and let us know!


local surge = {}


--- MATH FUNCTIONS ---


-- parity function returns 0 for even numbers and 1 for odd numbers
function math.parity(x)
    return (x % 2 == 1 and 1) or 0
end

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


-- WTSE MATH FUNCTIONS - NON-FINAL AND WIP


-- returns a table with the cumulative product of the elements in the input table
function math.cumprod(t)
    local o = {}
    o[1] = t[1]
    for i = 2, #t do
        o[i] = o[i - 1] * t[i]
    end
    return o
end

-- returns a table with the cumulative sum of the elements in the input table
function math.cumsum(t)
    local o = {}
    o[1] = t[1]
    for i = 2, #t do
        o[i] = o[i - 1] + t[i]
    end
    return o
end

-- returns a table containing num_points linearly spaced numbers from start_point to end_point
function math.linspace(start_point, end_point, num_points)
    local t = {}
    local step = (end_point - start_point) / (num_points - 1)
    for i = 1, num_points do
        t[i] = start_point + (i - 1) * step
    end
    return t
end

-- returns a table containing num_points logarithmically spaced numbers from 10^start_point to 10^end_point
function math.logspace(start_point, end_point, num_points)
    local t = {}
    local step = (end_point - start_point) / (num_points - 1)
    for i = 1, num_points do
        local exponent = start_point + (i - 1) * step
        t[i] = 10 ^ exponent
    end
    return t
end

-- returns the maximum absolute value found in the input table
function math.maxAbsFromTable(t)
    local o = 0
    for i = 1, #t do
            local a = abs(t[i])
            if a > o then o = a end
    end
    return o
end

-- returns the normalized sinc function for a table of input values
function math.sinc(t)
    local o = {}
    for i, x in ipairs(t) do
        if x == 0 then
            o[i] = 1
        else
            o[i] = math.sin(math.pi * x) / (math.pi * x)
        end
    end
    return o
end


--- END ---


return surge
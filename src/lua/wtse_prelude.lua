-- This document is loaded in each Surge XT session and provides a set of built-in helpers
-- we've found handy when generating wavetables. Consider it as a library of functions.
-- For each official update of Surge XT we will freeze the state of the prelude as stable
-- and not break included functions after that.
--
-- If you have ideas for other useful functions that could be added here, by all means
-- contact us over GitHub or Discord and let us know!


local surge = {}
local mod = {}


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


--- WTSE SPECIFIC MATH FUNCTIONS ---


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
    if num_points < 2 then
        return {start_point}
    end
    local t = {}
    local step = (end_point - start_point) / (num_points - 1)
    for i = 1, num_points do
        t[i] = start_point + (i - 1) * step
    end
    return t
end

-- returns a table containing num_points logarithmically spaced numbers from 10^start_point to 10^end_point
function math.logspace(start_point, end_point, num_points)
    if num_points < 2 then
        return {start_point}
    end
    local t = {}
    local step = (end_point - start_point) / (num_points - 1)
    for i = 1, num_points do
        local exponent = start_point + (i - 1) * step
        t[i] = 10 ^ exponent
    end
    return t
end

-- returns a table of length n, or a multidimensional table with {n, n, ..} dimensions all initialized with zeros
function math.zeros(dimensions)
    if type(dimensions) == "number" then
        dimensions = {dimensions}
    elseif type(dimensions) ~= "table" or #dimensions == 0 then
        return {0}
    end
    local function create_array(dimensions, depth)
        local size = dimensions[depth]
        local t = {}
        for i = 1, size do
            if depth < #dimensions then
                t[i] = create_array(dimensions, depth + 1)
            else
                t[i] = 0
            end
        end
        return t
    end
    return create_array(dimensions, 1)
end

-- returns a table or multidimensional table with every numerical value in the input table offset by x
function math.offset(t, x)
    local o = {}
    for k, v in pairs(t) do
        if type(v) == "table" then
            o[k] = math.offset(v, x)
        elseif type(v) == "number" then
            o[k] = v + x
        else
            o[k] = v
        end
    end
    return o
end

-- returns the maximum absolute value found in the input table
function math.max_abs(t)
    local o = 0
    for i = 1, #t do
            local a = math.abs(t[i])
            if a > o then o = a end
    end
    return o
end

-- deprecated: use math.max_abs() instead
function math.maxAbsFromTable(t)
    local o = 0
    for i = 1, #t do
            local a = math.abs(t[i])
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


--- BUILT-IN MODULATORS ---


-- returns a table or two dimensional table with values from the input table,
-- peak-normalized such that the maximum absolute value equals 1 (default) or the specified norm_factor
function mod.normalize_peaks(t, norm_factor)
    norm_factor = norm_factor or 1
    local max_val = 0
    local o = {}
    if type(t[1]) == "table" then
        for _, frame in ipairs(t) do
            max_val = math.max(max_val, math.max_abs(frame))
        end
    else
        max_val = math.max_abs(t)
    end
    if max_val > 0 then
        local scale_factor = norm_factor / max_val
        if type(t[1]) == "table" then
            for i, frame in ipairs(t) do
                o[i] = {}
                for j, value in ipairs(frame) do
                    o[i][j] = value * scale_factor
                end
            end
        else
            for i, value in ipairs(t) do
                o[i] = value * scale_factor
            end
        end
    else
        if type(t[1]) == "table" then
            o = math.zeros({#t, #t[1]})
        else
            o = math.zeros(#t)
        end
    end
    return o
end


--- END ---


surge.mod = mod

return surge

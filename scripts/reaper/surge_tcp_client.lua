-- Surge TCP Control Client for ReaScript
-- Usage: Load this module in your ReaScript to control Surge presets

local socket = require("socket")
local json = require("json") -- You may need to install json.lua

local SurgeClient = {}
SurgeClient.__index = SurgeClient

function SurgeClient:new(host, port)
    local self = setmetatable({}, SurgeClient)
    self.host = host or "127.0.0.1"
    self.port = port or 7833
    self.timeout = 1.0
    return self
end

function SurgeClient:sendCommand(command)
    local client = socket.tcp()
    client:settimeout(self.timeout)
    
    local success, err = client:connect(self.host, self.port)
    if not success then
        return nil, "Connection failed: " .. (err or "unknown error")
    end
    
    client:send(command)
    local response = client:receive("*l")
    client:close()
    
    if response then
        local ok, result = pcall(json.decode, response)
        if ok then
            return result
        else
            return {status = "error", message = "Invalid JSON response"}
        end
    else
        return nil, "No response from Surge"
    end
end

function SurgeClient:loadPreset(presetName)
    local cmd = "PRESET:LOAD:" .. presetName
    return self:sendCommand(cmd)
end

function SurgeClient:setParameter(paramId, value)
    local cmd = string.format("PARAM:SET:%d:%.4f", paramId, value)
    return self:sendCommand(cmd)
end

function SurgeClient:getPresetList()
    return self:sendCommand("PRESET:LIST")
end

function SurgeClient:ping()
    return self:sendCommand("PING")
end

-- ReaScript integration functions
function SurgeClient:loadPresetForTrack(trackIndex, presetName)
    local track = reaper.GetTrack(0, trackIndex)
    if not track then
        return false, "Track not found"
    end
    
    -- Find Surge instance on track
    local fxCount = reaper.TrackFX_GetCount(track)
    for i = 0, fxCount - 1 do
        local _, fxName = reaper.TrackFX_GetFXName(track, i, "")
        if string.find(fxName:lower(), "surge") then
            -- Load preset via TCP
            local result = self:loadPreset(presetName)
            if result and result.status == "ok" then
                return true, "Preset loaded: " .. presetName
            else
                return false, result and result.message or "Failed to load preset"
            end
        end
    end
    
    return false, "Surge not found on track"
end

-- Example usage in ReaScript:
--[[
local surge = SurgeClient:new()

-- Check connection
local status = surge:ping()
if status and status.status == "ok" then
    reaper.ShowConsoleMsg("Surge TCP control connected!\n")
    
    -- Load a preset
    local result = surge:loadPreset("DeepBass")
    if result.status == "ok" then
        reaper.ShowConsoleMsg("Loaded preset: " .. result.preset .. "\n")
    end
    
    -- Set a parameter
    surge:setParameter(10, 0.75) -- Set param 10 to 75%
    
    -- Get preset list
    local presets = surge:getPresetList()
    if presets.status == "ok" then
        for _, preset in ipairs(presets.presets) do
            reaper.ShowConsoleMsg("Available: " .. preset .. "\n")
        end
    end
else
    reaper.ShowConsoleMsg("Surge TCP control not available\n")
end
--]]

return SurgeClient
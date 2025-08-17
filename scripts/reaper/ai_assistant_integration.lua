-- AI Assistant Integration for Reaper + Surge
-- This demonstrates the full pipeline: MCP JS -> Lua -> Surge TCP

local SurgeClient = dofile(reaper.GetResourcePath() .. "/Scripts/surge_tcp_client.lua")

-- Initialize Surge client
local surge = SurgeClient:new()

-- Function called by your MCP JavaScript tool
function loadSurgePresetFromAI(presetName, trackIndex)
    -- Validate connection
    local status = surge:ping()
    if not status or status.status ~= "ok" then
        return {success = false, error = "Surge TCP control not available"}
    end
    
    -- Load the preset
    local result = surge:loadPreset(presetName)
    
    if result and result.status == "ok" then
        reaper.ShowConsoleMsg(string.format("AI Assistant: Loaded '%s' on track %d\n", 
                                           presetName, trackIndex))
        return {success = true, preset = presetName}
    else
        local errorMsg = result and result.message or "Unknown error"
        reaper.ShowConsoleMsg(string.format("AI Assistant: Failed to load preset - %s\n", 
                                           errorMsg))
        return {success = false, error = errorMsg}
    end
end

-- Function to get semantic preset recommendations
function getPresetsByCategory(category)
    local allPresets = surge:getPresetList()
    if not allPresets or allPresets.status ~= "ok" then
        return {}
    end
    
    -- Simple category matching (enhance with your AI logic)
    local categoryMap = {
        bass = {"Deep", "Sub", "Bass", "808"},
        pad = {"Pad", "Ambient", "Warm", "Soft"},
        lead = {"Lead", "Pluck", "Saw", "Square"},
        arp = {"Arp", "Seq", "Gate"},
        fx = {"FX", "Noise", "Sweep", "Riser"}
    }
    
    local keywords = categoryMap[category:lower()] or {}
    local matches = {}
    
    for _, preset in ipairs(allPresets.presets) do
        for _, keyword in ipairs(keywords) do
            if preset:lower():find(keyword:lower()) then
                table.insert(matches, preset)
                break
            end
        end
    end
    
    return matches
end

-- Example MCP bridge function
function processAICommand(command)
    -- Parse AI intent (this would come from your MCP JS tool)
    -- Example: {action = "load_preset", category = "bass", track = 0}
    
    if command.action == "load_preset" then
        if command.preset then
            -- Direct preset load
            return loadSurgePresetFromAI(command.preset, command.track or 0)
        elseif command.category then
            -- Category-based selection
            local presets = getPresetsByCategory(command.category)
            if #presets > 0 then
                -- Pick first match or use AI to select best
                return loadSurgePresetFromAI(presets[1], command.track or 0)
            else
                return {success = false, error = "No presets found for category: " .. command.category}
            end
        end
    elseif command.action == "morph_parameter" then
        -- Gradually change a parameter
        local startVal = command.start or 0
        local endVal = command.target or 1
        local steps = command.steps or 20
        
        for i = 0, steps do
            local value = startVal + (endVal - startVal) * (i / steps)
            surge:setParameter(command.param, value)
            reaper.defer(function() end) -- Small delay
        end
        
        return {success = true, param = command.param, value = endVal}
    end
    
    return {success = false, error = "Unknown command"}
end

-- Test the integration
function testIntegration()
    reaper.ShowConsoleMsg("=== AI Assistant Surge Integration Test ===\n")
    
    -- Test 1: Connection
    local ping = surge:ping()
    if ping and ping.status == "ok" then
        reaper.ShowConsoleMsg("✓ TCP connection established\n")
    else
        reaper.ShowConsoleMsg("✗ TCP connection failed\n")
        return
    end
    
    -- Test 2: List presets
    local presets = surge:getPresetList()
    if presets and presets.status == "ok" then
        reaper.ShowConsoleMsg(string.format("✓ Found %d presets\n", #presets.presets))
    else
        reaper.ShowConsoleMsg("✗ Failed to list presets\n")
    end
    
    -- Test 3: Simulate AI command
    local aiCommand = {
        action = "load_preset",
        category = "bass",
        track = 0
    }
    
    local result = processAICommand(aiCommand)
    if result.success then
        reaper.ShowConsoleMsg("✓ AI command processed successfully\n")
    else
        reaper.ShowConsoleMsg("✗ AI command failed: " .. (result.error or "unknown") .. "\n")
    end
    
    reaper.ShowConsoleMsg("=== Test Complete ===\n")
end

-- Run test if executed directly
if not ... then
    testIntegration()
end

return {
    loadPreset = loadSurgePresetFromAI,
    getPresetsByCategory = getPresetsByCategory,
    processAICommand = processAICommand
}
// MCP (Model Context Protocol) Tool for Surge Control
// This bridges between your AI agent and ReaScript

export class SurgeControlTool {
    constructor() {
        this.name = "surge_control";
        this.description = "Control Surge XT synthesizer presets and parameters";
    }

    // Tool schema for MCP
    getSchema() {
        return {
            name: this.name,
            description: this.description,
            parameters: {
                type: "object",
                properties: {
                    action: {
                        type: "string",
                        enum: ["load_preset", "set_parameter", "list_presets", "morph"],
                        description: "Action to perform on Surge"
                    },
                    preset: {
                        type: "string",
                        description: "Preset name to load (for load_preset action)"
                    },
                    category: {
                        type: "string",
                        enum: ["bass", "pad", "lead", "arp", "fx", "percussion"],
                        description: "Preset category for semantic search"
                    },
                    track: {
                        type: "integer",
                        description: "Track index in Reaper (0-based)"
                    },
                    parameter: {
                        type: "integer",
                        description: "Parameter ID to modify"
                    },
                    value: {
                        type: "number",
                        minimum: 0,
                        maximum: 1,
                        description: "Parameter value (0-1)"
                    }
                },
                required: ["action"]
            }
        };
    }

    // Execute the tool
    async execute(params) {
        try {
            // Build Lua command based on action
            let luaCode = '';
            
            switch (params.action) {
                case 'load_preset':
                    if (params.preset) {
                        luaCode = `
                            local ai = dofile(reaper.GetResourcePath() .. "/Scripts/ai_assistant_integration.lua")
                            local result = ai.loadPreset("${params.preset}", ${params.track || 0})
                            return result.success and "OK:" .. result.preset or "ERROR:" .. result.error
                        `;
                    } else if (params.category) {
                        luaCode = `
                            local ai = dofile(reaper.GetResourcePath() .. "/Scripts/ai_assistant_integration.lua")
                            local result = ai.processAICommand({
                                action = "load_preset",
                                category = "${params.category}",
                                track = ${params.track || 0}
                            })
                            return result.success and "OK" or "ERROR:" .. result.error
                        `;
                    }
                    break;
                    
                case 'set_parameter':
                    luaCode = `
                        local SurgeClient = dofile(reaper.GetResourcePath() .. "/Scripts/surge_tcp_client.lua")
                        local surge = SurgeClient:new()
                        local result = surge:setParameter(${params.parameter}, ${params.value})
                        return result.status == "ok" and "OK" or "ERROR"
                    `;
                    break;
                    
                case 'list_presets':
                    luaCode = `
                        local SurgeClient = dofile(reaper.GetResourcePath() .. "/Scripts/surge_tcp_client.lua")
                        local surge = SurgeClient:new()
                        local result = surge:getPresetList()
                        if result.status == "ok" then
                            return table.concat(result.presets, ",")
                        else
                            return "ERROR"
                        end
                    `;
                    break;
                    
                case 'morph':
                    luaCode = `
                        local ai = dofile(reaper.GetResourcePath() .. "/Scripts/ai_assistant_integration.lua")
                        local result = ai.processAICommand({
                            action = "morph_parameter",
                            param = ${params.parameter},
                            target = ${params.value},
                            steps = 20
                        })
                        return result.success and "OK" or "ERROR"
                    `;
                    break;
            }
            
            // Execute via ReaScript bridge
            const result = await this.executeReaScript(luaCode);
            
            if (result.startsWith("OK")) {
                return {
                    success: true,
                    message: `Successfully executed ${params.action}`,
                    data: result.substring(3)
                };
            } else {
                return {
                    success: false,
                    error: result.substring(6) || "Operation failed"
                };
            }
            
        } catch (error) {
            return {
                success: false,
                error: error.message
            };
        }
    }
    
    // Bridge to ReaScript execution
    async executeReaScript(luaCode) {
        // This would be implemented based on your specific bridge mechanism
        // Options include:
        // 1. Write to temp file and execute via Reaper API
        // 2. Use OSC to trigger ReaScript
        // 3. Use a custom bridge process
        
        // Example pseudo-code:
        const tempFile = `/tmp/surge_control_${Date.now()}.lua`;
        await fs.writeFile(tempFile, luaCode);
        
        // Trigger execution in Reaper (implementation depends on your setup)
        const result = await this.reaperBridge.runScript(tempFile);
        
        await fs.unlink(tempFile);
        return result;
    }
}

// Example usage in your AI agent
export async function handleUserIntent(intent) {
    const surgeTool = new SurgeControlTool();
    
    // Parse natural language to tool parameters
    let params = {};
    
    if (intent.includes("bass sound") || intent.includes("deep bass")) {
        params = {
            action: "load_preset",
            category: "bass",
            track: 0
        };
    } else if (intent.includes("warm pad")) {
        params = {
            action: "load_preset",
            preset: "WarmPad",
            track: 0
        };
    } else if (intent.includes("brighten") || intent.includes("filter")) {
        params = {
            action: "set_parameter",
            parameter: 10, // Assuming param 10 is filter cutoff
            value: 0.8
        };
    }
    
    const result = await surgeTool.execute(params);
    
    if (result.success) {
        return `I've ${params.action === 'load_preset' ? 'loaded the preset' : 'adjusted the parameter'} as requested.`;
    } else {
        return `I encountered an issue: ${result.error}`;
    }
}

// Export for MCP registration
export default {
    tools: [new SurgeControlTool()]
};
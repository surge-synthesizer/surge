# Surge TCP Control Integration Build Instructions

## Overview
This modification adds a TCP control interface to Surge XT, enabling your AI assistant to programmatically control presets and parameters through the pipeline:
**NLP → MCP JavaScript → Lua/ReaScript → Surge TCP**

## Prerequisites
- Surge XT source code from GitHub
- C++ build environment (Visual Studio on Windows, Xcode on macOS, GCC on Linux)
- CMake 3.15+
- Reaper with ReaScript enabled
- LuaSocket library for Reaper

## Step 1: Fork and Clone Surge
```bash
git clone https://github.com/surge-synthesizer/surge.git
cd surge
git checkout main
```

## Step 2: Add TCP Controller Files
Copy these files to the Surge source tree:
```
surge/src/common/TCPControl/SurgeTCPController.h
surge/src/common/TCPControl/SurgeTCPController.cpp
```

## Step 3: Modify CMakeLists.txt
Add to `surge/CMakeLists.txt`:
```cmake
set(SURGE_TCP_CONTROL_SOURCES
    src/common/TCPControl/SurgeTCPController.cpp
    src/common/TCPControl/SurgeTCPController.h
)

target_sources(surge-common PRIVATE ${SURGE_TCP_CONTROL_SOURCES})
```

## Step 4: Integrate with SurgeSynthesizer
Edit `surge/src/common/SurgeSynthesizer.h`:
```cpp
// Add include
#include "TCPControl/SurgeTCPController.h"

// Add member variable
std::unique_ptr<Surge::TCPControl::TCPController> tcpController;
```

Edit `surge/src/common/SurgeSynthesizer.cpp`:
```cpp
// In constructor, add:
initializeTCPControl(); // Add the function from SurgeIntegration.cpp

// In destructor, add:
if (tcpController) {
    tcpController->stop();
}
```

## Step 5: Build Surge
```bash
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target surge-xt_VST3
```

## Step 6: Install Modified Surge
- Copy the built VST3 to your plugin folder
- Verify it loads in Reaper

## Step 7: Setup Reaper Scripts
1. Copy Lua files to Reaper Scripts folder:
   - `surge_tcp_client.lua`
   - `ai_assistant_integration.lua`

2. Install LuaSocket for Reaper:
   - Download from: https://luarocks.org/modules/luasocket/luasocket
   - Place in Reaper's Lua folder

## Step 8: Setup MCP Bridge
1. Install your MCP server with the JavaScript bridge
2. Configure `mcp_surge_bridge.js` with your specific bridge mechanism

## Testing
1. Load Surge in Reaper
2. Run the test script:
```lua
-- In Reaper's Actions window, run:
dofile(reaper.GetResourcePath() .. "/Scripts/ai_assistant_integration.lua")
```

3. Check Reaper console for test results

## Security Notes
- TCP listener is bound to localhost only (127.0.0.1)
- No authentication implemented (add if needed for production)
- Consider using Unix sockets instead of TCP for better security

## Troubleshooting
- **Port 7833 in use**: Change port in both C++ and Lua files
- **Connection refused**: Ensure Surge is loaded and TCP listener started
- **Preset not found**: Check preset names match exactly
- **LuaSocket not found**: Verify LuaSocket installation in Reaper

## Maintenance
When updating Surge:
```bash
git fetch upstream
git merge upstream/main
# Resolve any conflicts in your TCP control code
cmake --build build
```
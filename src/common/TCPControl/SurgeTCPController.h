#pragma once

#include <thread>
#include <atomic>
#include <string>
#include <functional>

namespace Surge {
namespace TCPControl {

class TCPController {
public:
    TCPController();
    ~TCPController();
    
    void start(int port = 7833);
    void stop();
    
    using PresetCallback = std::function<bool(const std::string&)>;
    using ParamCallback = std::function<bool(int, float)>;
    using ListCallback = std::function<std::vector<std::string>()>;
    
    void setPresetLoadCallback(PresetCallback cb) { presetLoadCb = cb; }
    void setParamSetCallback(ParamCallback cb) { paramSetCb = cb; }
    void setPresetListCallback(ListCallback cb) { presetListCb = cb; }
    
private:
    void listenLoop();
    std::string processCommand(const std::string& cmd);
    std::string jsonResponse(bool success, const std::string& data = "");
    
    std::thread listenerThread;
    std::atomic<bool> running{false};
    int serverSocket{-1};
    int port{7833};
    
    PresetCallback presetLoadCb;
    ParamCallback paramSetCb;
    ListCallback presetListCb;
};

} // namespace TCPControl
} // namespace Surge
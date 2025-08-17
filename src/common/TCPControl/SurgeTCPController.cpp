#include "SurgeTCPController.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <vector>

namespace Surge {
namespace TCPControl {

TCPController::TCPController() {}

TCPController::~TCPController() {
    stop();
}

void TCPController::start(int portNum) {
    if (running) return;
    
    port = portNum;
    running = true;
    listenerThread = std::thread(&TCPController::listenLoop, this);
}

void TCPController::stop() {
    if (!running) return;
    
    running = false;
    if (serverSocket >= 0) {
        close(serverSocket);
    }
    if (listenerThread.joinable()) {
        listenerThread.join();
    }
}

void TCPController::listenLoop() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) return;
    
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // localhost only
    serverAddr.sin_port = htons(port);
    
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(serverSocket);
        return;
    }
    
    listen(serverSocket, 5);
    
    while (running) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (clientSocket < 0) continue;
        
        char buffer[1024] = {0};
        int bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
        
        if (bytesRead > 0) {
            std::string response = processCommand(std::string(buffer));
            write(clientSocket, response.c_str(), response.length());
        }
        
        close(clientSocket);
    }
    
    close(serverSocket);
}

std::string TCPController::processCommand(const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string token;
    std::vector<std::string> tokens;
    
    while (std::getline(iss, token, ':')) {
        tokens.push_back(token);
    }
    
    if (tokens.empty()) {
        return jsonResponse(false, "Invalid command");
    }
    
    if (tokens[0] == "PING") {
        return "{\"status\":\"ok\",\"version\":\"1.0\"}";
    }
    
    if (tokens[0] == "PRESET" && tokens.size() >= 2) {
        if (tokens[1] == "LOAD" && tokens.size() >= 3) {
            if (presetLoadCb) {
                bool success = presetLoadCb(tokens[2]);
                if (success) {
                    return "{\"status\":\"ok\",\"preset\":\"" + tokens[2] + "\"}";
                } else {
                    return jsonResponse(false, "Failed to load preset: " + tokens[2]);
                }
            }
            return jsonResponse(false, "Preset load callback not set");
        }
        
        if (tokens[1] == "LIST") {
            if (presetListCb) {
                auto presets = presetListCb();
                std::string json = "{\"status\":\"ok\",\"presets\":[";
                for (size_t i = 0; i < presets.size(); ++i) {
                    json += "\"" + presets[i] + "\"";
                    if (i < presets.size() - 1) json += ",";
                }
                json += "]}";
                return json;
            }
            return jsonResponse(false, "Preset list callback not set");
        }
    }
    
    if (tokens[0] == "PARAM" && tokens.size() >= 4 && tokens[1] == "SET") {
        if (paramSetCb) {
            try {
                int paramId = std::stoi(tokens[2]);
                float value = std::stof(tokens[3]);
                bool success = paramSetCb(paramId, value);
                if (success) {
                    return "{\"status\":\"ok\",\"param\":\"" + tokens[2] + 
                           "\",\"value\":" + tokens[3] + "}";
                }
            } catch (...) {
                return jsonResponse(false, "Invalid parameter format");
            }
        }
        return jsonResponse(false, "Param set callback not set");
    }
    
    return jsonResponse(false, "Unknown command: " + tokens[0]);
}

std::string TCPController::jsonResponse(bool success, const std::string& data) {
    if (success) {
        return "{\"status\":\"ok\",\"data\":\"" + data + "\"}";
    } else {
        return "{\"status\":\"error\",\"message\":\"" + data + "\"}";
    }
}

} // namespace TCPControl
} // namespace Surge
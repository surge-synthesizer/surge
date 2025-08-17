// Add this to SurgeSynthesizer.cpp or your main plugin class

#include "SurgeTCPController.h"

// In your SurgeSynthesizer class header, add:
// std::unique_ptr<Surge::TCPControl::TCPController> tcpController;

// In constructor or initialization:
void SurgeSynthesizer::initializeTCPControl() {
    tcpController = std::make_unique<Surge::TCPControl::TCPController>();
    
    // Hook up preset loading
    tcpController->setPresetLoadCallback([this](const std::string& presetName) -> bool {
        // Find preset by name in storage
        auto* storage = this->storage.getPatch();
        
        // Search through factory and user presets
        for (const auto& category : storage->preset_categories) {
            for (const auto& preset : category.presets) {
                if (preset.name == presetName) {
                    // Load the preset using Surge's internal method
                    this->loadPatchFromFile(preset.path);
                    return true;
                }
            }
        }
        
        // Try direct file path if not found by name
        if (fs::exists(presetName)) {
            this->loadPatchFromFile(presetName);
            return true;
        }
        
        return false;
    });
    
    // Hook up parameter setting
    tcpController->setParamSetCallback([this](int paramId, float value) -> bool {
        if (paramId >= 0 && paramId < n_total_params) {
            // Set parameter through Surge's automation system
            this->setParameter01(paramId, value, true);
            return true;
        }
        return false;
    });
    
    // Hook up preset listing
    tcpController->setPresetListCallback([this]() -> std::vector<std::string> {
        std::vector<std::string> presetNames;
        auto* storage = this->storage.getPatch();
        
        for (const auto& category : storage->preset_categories) {
            for (const auto& preset : category.presets) {
                presetNames.push_back(preset.name);
            }
        }
        
        return presetNames;
    });
    
    // Start the TCP listener
    tcpController->start(7833);
}

// In destructor:
void SurgeSynthesizer::cleanup() {
    if (tcpController) {
        tcpController->stop();
    }
}
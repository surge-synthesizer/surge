#pragma once

#include <iostream>
#include <iomanip>

class HeadlessPluginLayerProxy
{
public:
    void updateDisplay()
    {
        std::cerr << "HeadlessPluginLayerProxy::updateDisplay" << std::endl;
    }
};

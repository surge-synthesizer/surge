#pragma once

#include <iostream>
#include <iomanip>

class HeadlessPluginLayerProxy : public SurgeSynthesizer::PluginLayer
{
  public:
    void surgeParameterUpdated(const SurgeSynthesizer::ID &id, float d) override {}
    void surgeMacroUpdated(long macroNum, float d) override {}
};

//
// Created by Paul Walker on 4/24/21.
//

#ifndef SURGE_XT_SURGEJUCELOOKANDFEEL_H
#define SURGE_XT_SURGEJUCELOOKANDFEEL_H

#include <JuceHeader.h>

class SurgeJUCELookAndFeel : public juce::LookAndFeel_V4
{
  public:
    void drawLabel(juce::Graphics &graphics, juce::Label &label) override;
};

#endif // SURGE_XT_SURGEJUCELOOKANDFEEL_H

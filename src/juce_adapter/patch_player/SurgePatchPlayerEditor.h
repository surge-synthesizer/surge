// -*-c++-*-
/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/


#pragma once

#include <JuceHeader.h>
#include "SurgePatchPlayerProcessor.h"

struct sppCategoryModel;
struct sppPatchModel;

//==============================================================================
/**
*/
class SurgePatchPlayerAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    SurgePatchPlayerAudioProcessorEditor (SurgePatchPlayerAudioProcessor&);
    ~SurgePatchPlayerAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

   virtual void timerCallback() override {
      idle();
   }


   void categorySelected( int row );
   void patchSelected( int row );

   void idle();
   
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SurgePatchPlayerAudioProcessor& audioProcessor;

private:

   std::unique_ptr<juce::ListBox> categoryBox, patchBox;
   std::unique_ptr<sppCategoryModel> categoryModel;
   std::unique_ptr<sppPatchModel> patchModel;
   std::unique_ptr<juce::Component> vuMeter;

   std::unique_ptr<juce::Label> patchName, patchAuthor;

   std::unique_ptr<juce::Label>   currentPatch;

   std::unique_ptr<juce::Drawable> surgeLogo;
   juce::Typeface::Ptr latoTF;
   juce::Font latoFont;

   int patchId;
   
   static constexpr int logoSize = 64;
   static constexpr int logoMargin = 4;
   static constexpr int patchAndVU = 64 + 24;
   static constexpr int controlsAndMacros = 128;
   static constexpr int aboutInfo = 20;

   
   friend struct sppCategoryModel;
   friend struct sppPatchModel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SurgePatchPlayerAudioProcessorEditor)
};

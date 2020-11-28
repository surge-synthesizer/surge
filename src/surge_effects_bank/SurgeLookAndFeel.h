// -*- mode: c++-mode -*-

#include "../JuceLibraryCode/JuceHeader.h"
#include "version.h"

class SurgeLookAndFeel : public LookAndFeel_V4
{
private:
    std::unique_ptr<Drawable> surgeLogo;

public:

    enum SurgeColourIds
    {
        componentBgStart = 0x2700001,
        componentBgEnd,
        
        orange,
        orangeMedium,
        orangeDark, 
        blue,

        knobEdge,
        knobBg,
        knobHandle,

        knobEdgeDisable,
        knobBgDisable,
        knobHandleDisable,

        paramEnabledBg,
        paramEnabledEdge,
        paramDisabledBg,
        paramDisabledEdge,
        paramDisplay
    };

    SurgeLookAndFeel() {
        Colour surgeGrayBg = Colour(205,206,212);
        Colour surgeOrange = Colour(255,144,0);
        Colour surgeBlue = Colour(18,52,99);
        Colour white = Colour(255,255,255);
        Colour black = Colour(0,0,0);
        Colour surgeOrangeDark = Colour(101,50,3);
        Colour surgeOrangeMedium = Colour(227,112,8);
        
        setColour(SurgeColourIds::componentBgStart, surgeGrayBg.interpolatedWith(surgeOrangeMedium, 0.1));
        setColour(SurgeColourIds::componentBgEnd, surgeGrayBg);
        setColour(SurgeColourIds::orange, surgeOrange);
        setColour(SurgeColourIds::orangeDark, surgeOrangeDark);
        setColour(SurgeColourIds::orangeMedium, surgeOrangeMedium);
        setColour(SurgeColourIds::blue, surgeBlue);

        setColour(SurgeColourIds::knobHandle, white);
        setColour(SurgeColourIds::knobBg, surgeOrange);
        setColour(SurgeColourIds::knobEdge, surgeBlue);

        auto disableOpacity = 0.55;
        setColour(SurgeColourIds::knobHandleDisable, surgeBlue.interpolatedWith(surgeGrayBg, disableOpacity));
        setColour(SurgeColourIds::knobBgDisable, surgeOrange.interpolatedWith(surgeGrayBg, disableOpacity));
        setColour(SurgeColourIds::knobEdgeDisable, surgeBlue);

        setColour(SurgeColourIds::paramEnabledBg, black);
        setColour(SurgeColourIds::paramEnabledEdge, surgeOrange);
        setColour(SurgeColourIds::paramDisabledBg, black.interpolatedWith(surgeGrayBg, disableOpacity));
        setColour(SurgeColourIds::paramDisabledEdge, surgeBlue);
        setColour(SurgeColourIds::paramDisplay, white);
        
        surgeLogo = Drawable::createFromImageData (BinaryData::SurgeLogoOnlyBlue_svg, BinaryData::SurgeLogoOnlyBlue_svgSize);
    }


    virtual void drawRotarySlider(Graphics &g,
                                  int x, int y, int width, int height,
                                  float sliderPos,
                                  float rotaryStartAngle, float rotaryEndAngle,
                                  Slider &slider) override
    {
        auto fill = findColour(SurgeColourIds::knobBg);
        auto edge = findColour(SurgeColourIds::knobEdge);
        auto tick = findColour(SurgeColourIds::knobHandle);

        if( ! slider.isEnabled() )
        {
            fill = findColour(SurgeColourIds::knobBgDisable);
            edge = findColour(SurgeColourIds::knobEdgeDisable);
            tick = findColour(SurgeColourIds::knobHandleDisable);
        }
        

        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (10);
        g.setColour(fill);
        g.fillEllipse(bounds);
        g.setColour(edge);
        g.drawEllipse(bounds, 1.0);

        
        auto radius = jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto arcRadius = radius;
        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        auto thumbWidth = 5;

        juce::Point<float> thumbPoint (bounds.getCentreX() + arcRadius * std::cos (toAngle - MathConstants<float>::halfPi),
                                 bounds.getCentreY() + arcRadius * std::sin (toAngle - MathConstants<float>::halfPi));
        
        g.setColour (tick);
        g.fillEllipse (juce::Rectangle<float> (thumbWidth, thumbWidth).withCentre (thumbPoint));
        g.setColour(edge);
        g.drawEllipse (juce::Rectangle<float> (thumbWidth, thumbWidth).withCentre (thumbPoint), 1.0);
        g.setColour(tick);
        g.fillEllipse (juce::Rectangle<float> (thumbWidth, thumbWidth).withCentre( bounds.getCentre() ) );

        auto l = Line<float>(thumbPoint, bounds.getCentre());
        g.drawLine(l, thumbWidth );
    }

    virtual void drawButtonBackground(Graphics &g,
                                      Button &button,
                                      const Colour &backgroundColour,
                                      bool 	shouldDrawButtonAsHighlighted,
                                      bool 	shouldDrawButtonAsDown
        ) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (2.f, 2.f);
        auto col    = findColour(SurgeColourIds::orangeDark);
        auto edge   = findColour(SurgeColourIds::blue);

        if( shouldDrawButtonAsHighlighted )
            col = Colour(18 * 1.4, 52 * 1.4, 99 * 1.4);

        if( shouldDrawButtonAsDown )
            col = Colour(18 * 1.2, 52 * 1.2, 99 * 1.2);

        if (button.getToggleState() )
            col = findColour(SurgeColourIds::orange);
        
        g.setColour(col);
        g.fillRoundedRectangle(bounds, 3);
        g.setColour(edge);
        g.drawRoundedRectangle(bounds, 3, 1);

    }
    
    void paintComponentBackground(Graphics &g, int w, int h)
    {
        int orangeHeight = 20;

        g.fillAll(findColour(SurgeColourIds::componentBgStart));

        ColourGradient cg(findColour(SurgeColourIds::componentBgStart), 0, 0,
                          findColour(SurgeColourIds::componentBgEnd), 0, h-orangeHeight,
                          false);
        g.setGradientFill(cg);
        g.fillRect(0,0,w,h-orangeHeight);

        g.setColour(findColour(SurgeColourIds::orange));
        g.fillRect(0,h-orangeHeight,w,orangeHeight);
        
        juce::Rectangle<float> logoBound { w/2.f-30, h-orangeHeight + 2.f, 60, orangeHeight - 4.f };
        surgeLogo->drawWithin(g, logoBound, juce::RectanglePlacement::xMid | juce::RectanglePlacement::yMid, 1.0 );
        
        g.setColour(findColour(SurgeColourIds::blue));
        g.drawLine(0,h-orangeHeight,w,h-orangeHeight);
        // text
        g.setFont(12);
        g.drawSingleLineText( Surge::Build::FullVersionStr, 3, h - 6.f, juce::Justification::left );
        g.drawSingleLineText( Surge::Build::BuildDate, w - 3, h - 6.f, juce::Justification::right );

    }
    
};

class SurgeFXParamDisplay : public Component
{
public:
    virtual void setGroup( std::string grp ) { group = grp; repaint(); };
    virtual void setName( std::string nm ) { name = nm; repaint(); }
    virtual void setDisplay( std::string dis ) { display = dis; repaint(); };
    
    virtual void paint(Graphics &g)
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.f, 2.f);
        auto edge = findColour(SurgeLookAndFeel::SurgeColourIds::paramEnabledEdge);

        if( isEnabled() )
            g.setColour(findColour(SurgeLookAndFeel::SurgeColourIds::paramEnabledBg));
        else
        {
            g.setColour(findColour(SurgeLookAndFeel::SurgeColourIds::paramDisabledBg));
            edge = findColour(SurgeLookAndFeel::SurgeColourIds::paramDisabledEdge);
        }
        
        g.fillRoundedRectangle(bounds, 5);
        g.setColour(edge);
        g.drawRoundedRectangle(bounds, 5, 1);

        if( isEnabled() )
        {
            g.setColour(findColour(SurgeLookAndFeel::SurgeColourIds::paramDisplay));
            g.setFont(10);
            g.drawSingleLineText( group, bounds.getX() + 5, bounds.getY() + 2 + 10 );
            g.setFont(12);
            g.drawSingleLineText( name, bounds.getX() + 5, bounds.getY() + 2 + 10 + 3 + 11 );
            
            g.setFont(20);
            g.drawSingleLineText( display, bounds.getX() + 5, bounds.getY() + bounds.getHeight() - 5 );
        }
    }

private: 
    std::string group = "Uninit";
    std::string name = "Uninit";
    std::string display = "SoftwareError";
};

class SurgeTempoSyncSwitch : public ToggleButton {
protected:
    virtual void paintButton(Graphics &g,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override {
        auto bounds = getLocalBounds().toFloat().reduced (1.f, 1.f);
        auto edge = findColour(SurgeLookAndFeel::SurgeColourIds::paramEnabledEdge);
        auto handle = findColour(SurgeLookAndFeel::SurgeColourIds::orange);
        float radius = 5;
        
        if( isEnabled() )
            g.setColour(findColour(SurgeLookAndFeel::SurgeColourIds::paramEnabledBg));
        else
        {
            g.setColour(findColour(SurgeLookAndFeel::SurgeColourIds::paramDisabledBg));
            edge = findColour(SurgeLookAndFeel::SurgeLookAndFeel::paramDisabledEdge);
        }
        
        g.fillRoundedRectangle(bounds, radius);
        g.setColour(edge);
        g.drawRoundedRectangle(bounds, radius, 1);

        if( ! isEnabled() ) return;
        
        float controlRadius = bounds.getWidth() - 3;
        float yPos;
        float xPos = bounds.getX() + bounds.getWidth() / 2.0 - controlRadius / 2.0;
        if( getToggleState() )
        {
            yPos = bounds.getY() + 2;
        }
        else
        {
            yPos = bounds.getY() + bounds.getHeight() - controlRadius - 2;
        }
        auto kbounds = juce::Rectangle<float> (xPos, yPos, controlRadius, controlRadius);
        g.setColour(handle);
        g.fillEllipse(kbounds);

    }
};


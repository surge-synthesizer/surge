src/surge-xt/gui/overlays/PatchStoreDialog.cpp:    juce::Font font{12};
src/surge-xt/gui/overlays/KeyBindingsOverlay.cpp:        keyDesc->setFont(juce::Font(10));
src/surge-xt/gui/SurgeJUCELookAndFeel.h:    juce::Font getPopupMenuFont() override;
src/surge-xt/gui/SurgeJUCELookAndFeel.h:    juce::Font getPopupMenuBoldFont();
src/surge-xt/gui/SurgeGUIEditor.cpp:                hs->setFont(juce::Font(currentSkin->typeFaces[ff]).withPointHeight(fs));
src/surge-xt/gui/SkinSupport.cpp:        return juce::Font::FontStyleFlags::bold;
src/surge-xt/gui/SkinSupport.cpp:        return juce::Font::FontStyleFlags::italic;
src/surge-xt/gui/SkinSupport.cpp:        return juce::Font::FontStyleFlags::underlined;
src/surge-xt/gui/SkinSupport.cpp:        return juce::Font::FontStyleFlags::plain;
src/surge-xt/gui/SkinSupport.cpp:    jassert((int)Surge::Skin::FontDesc::plain == juce::Font::FontStyleFlags::plain);
src/surge-xt/gui/SkinSupport.cpp:    jassert((int)Surge::Skin::FontDesc::italic == juce::Font::FontStyleFlags::italic);
src/surge-xt/gui/SkinSupport.cpp:    jassert((int)Surge::Skin::FontDesc::bold == juce::Font::FontStyleFlags::bold);
src/surge-xt/gui/SkinSupport.cpp:            return juce::Font(typeFaces[fo.family]).withPointHeight(fo.size);
src/surge-xt/gui/SkinSupport.cpp:        return juce::Font(fo.family, fo.size, juce::Font::FontStyleFlags::plain);
src/surge-xt/gui/RuntimeFont.h:    juce::Font
src/surge-xt/gui/RuntimeFont.h:    juce::Font
src/surge-xt/gui/widgets/XMLConfiguredMenus.h:    juce::Font::FontStyleFlags font_style{juce::Font::plain};
src/surge-xt/gui/widgets/MultiSwitch.h:    juce::Font font{36};
src/surge-xt/gui/widgets/ModulationSourceButton.h:    juce::Font font;
src/surge-xt/gui/widgets/ParameterInfowindow.h:    juce::Font font;
src/surge-xt/gui/widgets/ModulatableControlInterface.h:    juce::Font font;
src/surge-xt/gui/widgets/ModulatableControlInterface.h:    virtual void setFont(juce::Font f) { font = f; }
src/surge-xt/gui/widgets/VerticalLabel.h:    juce::Font font;
src/surge-xt/gui/SurgeJUCELookAndFeel.cpp:    auto fontVersion = skin->fontManager->getFiraMonoAtSize(14, juce::Font::bold);
src/surge-xt/gui/SurgeJUCELookAndFeel.cpp:juce::Font SurgeJUCELookAndFeel::getPopupMenuFont()
src/surge-xt/gui/SurgeJUCELookAndFeel.cpp:juce::Font SurgeJUCELookAndFeel::getPopupMenuBoldFont()
src/surge-xt/gui/SurgeJUCELookAndFeel.cpp:    // return juce::Font("Comic Sans MS", 15, juce::Font::plain);

#include <algorithm>
#include "juce_audio_formats/juce_audio_formats.h"

#include "CurrentFxDisplay.h"

#include "AccessibleHelpers.h"
#include "EffectChooser.h"
#include "RuntimeFont.h"
#include "SkinModel.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUIEditorTags.h"
#include "dsp/effects/ConditionerEffect.h"
#include "dsp/effects/ConvolutionEffect.h"

namespace
{
const int yofs = 10;
} // namespace

namespace Surge
{
namespace Widgets
{

namespace
{

struct ConvolutionButton : public juce::Component
{
    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    SurgeGUIEditor *sge{nullptr};
    void setSurgeGUIEditor(SurgeGUIEditor *s) { sge = s; }

    ConvolutionEffect *fx{nullptr};
    FxStorage *fxs{nullptr};
    void setEffect(ConvolutionEffect *f, FxStorage *s)
    {
        fx = f;
        fxs = s;
    }

    bool isMousedOver = false;

    void mouseEnter(const juce::MouseEvent &event) override
    {
        isMousedOver = true;
        repaint();
    }
    void mouseExit(const juce::MouseEvent &event) override
    {
        isMousedOver = false;
        repaint();
    }

    void paint(juce::Graphics &g) override
    {
        if (!fx)
            return;

        auto skin = sge->currentSkin;

        auto fgcol = skin->getColor(Colors::Effect::Grid::Scene::Background);
        auto fgframe = skin->getColor(Colors::Effect::Grid::Scene::Border);
        auto fgtext = skin->getColor(Colors::Effect::Grid::Scene::Text);

        auto fgcolHov = skin->getColor(Colors::Effect::Grid::Scene::BackgroundHover);
        auto fgframeHov = skin->getColor(Colors::Effect::Grid::Scene::BackgroundHover);
        auto fgtextHov = skin->getColor(Colors::Effect::Grid::Scene::TextHover);

        auto irr = getLocalBounds();

        std::string irname("No IR loaded.");
        if (fx->initialized)
        {
            irname = fxs->by_key("irname").to_string();
        }
        g.setColour(isMousedOver ? fgcolHov : fgcol);
        g.fillRect(irr);
        g.setColour(isMousedOver ? fgframeHov : fgframe);
        g.drawRect(irr);
        g.setColour(isMousedOver ? fgtextHov : fgtext);
        g.setFont(skin->fontManager->getLatoAtSize(9));
        g.drawText(juce::String(irname), irr, juce::Justification::centred);
    }
};

} // namespace

CurrentFxDisplay::CurrentFxDisplay(SurgeGUIEditor *e)
{
    editor_ = e;
    storage = e->getStorage();
}

CurrentFxDisplay::~CurrentFxDisplay() {}

void CurrentFxDisplay::renderCurrentFx()
{
    switch (storage->getPatch().fx[current_fx_].type.val.i)
    {
    case fxt_conditioner:
        conditionerRender();
        break;
    default:
        break;
    }
}

void CurrentFxDisplay::updateCurrentFx(int current_fx)
{
    std::lock_guard<std::mutex> g(editor_->synth->fxSpawnMutex);
    current_fx_ = current_fx;
    effect_ = editor_->synth->fx[current_fx].get();

    switch (storage->getPatch().fx[current_fx].type.val.i)
    {
    case fxt_vocoder:
        vocoderLayout();
        break;
    case fxt_conditioner:
        conditionerLayout();
        break;
    case fxt_convolution:
        convolutionLayout();
        break;
    default:
        defaultLayout();
        break;
    };
}

void CurrentFxDisplay::defaultLayout()
{
    std::unordered_map<int, int> paramToParamIndex;
    Surge::GUI::Skin::ptr_t currentSkin = editor_->currentSkin;
    FxStorage &fx = storage->getPatch().fx[current_fx_];

    // Sizing for the labels.
    auto label_rect = fxRect();

    // This is kinda dumb, but we have to provide an absolute param index number
    // to layoutComponentForSkin, so figure it out here.
    int i = 0;
    // IDs of others that we need to lay out.
    int fx_type = -1;

    std::vector<Parameter *>::iterator iter;
    for (iter = editor_->synth->storage.getPatch().param_ptr.begin();
         iter != editor_->synth->storage.getPatch().param_ptr.end(); iter++, i++)
    {
        Parameter *p = *iter;
        if (strcmp(p->ui_identifier, "fx.type") == 0)
        {
            fx_type = i;
            continue;
        }

        for (int j = 0; j < n_fx_params; j++)
        {
            if (p == &fx.p[j])
            {
                paramToParamIndex[j] = i;
                break;
            }
        }
    }

    // Code to lay out the parameter and wire it up.
    auto makeParameter = [this, &currentSkin](int n) {
        Parameter *p = editor_->synth->storage.getPatch().param_ptr[n];
        Surge::Skin::Connector conn = Surge::Skin::Connector::connectorByID(p->ui_identifier);
        if (p->hasSkinConnector && conn.payload->defaultComponent != Surge::Skin::Components::None)
        {
            auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);
            currentSkin->resolveBaseParentOffsets(skinCtrl);
            auto comp =
                editor_->layoutComponentForSkin(skinCtrl, p->id + start_paramtags, n, p,
                                                p->ctrlstyle | conn.payload->controlStyleFlags);
            uiidToSliderLabel[p->ui_identifier] = p->get_name();
            if (p->id == editor_->synth->learn_param_from_cc ||
                p->id == editor_->synth->learn_param_from_note)
            {
                editor_->showMidiLearnOverlay(editor_->param[p->id]
                                                  ->asControlValueInterface()
                                                  ->asJuceComponent()
                                                  ->getBounds());
            }
        }
    };

    // Start placing the various components.
    layoutJogFx();
    layoutFxSelector();
    layoutFxPresetLabel();
    // FX menu. This one's a little weird: the construction is done in
    // layoutComponentForSkin.
    makeParameter(fx_type);

    // Now the actual FX labels and parameter.
    if (effect_)
    {
        for (i = 0; i < n_fx_params; i++)
        {
            // First, a new group?
            const char *label = effect_->group_label(i);
            if (label)
            {
                auto vr = label_rect.withTrimmedTop(-1)
                              .withTrimmedRight(-5)
                              .translated(5, -12)
                              .translated(0, yofs * effect_->group_label_ypos(i));

                if (!labels_[i])
                {
                    labels_[i] = std::make_unique<Surge::Widgets::EffectLabel>();
                }

                labels_[i]->setBounds(vr);
                labels_[i]->setSkin(currentSkin, editor_->bitmapStore);
                labels_[i]->setLabel(label);

                // This looks weird: we're putting the label into the editor's
                // MainFrame instead of our own component? That's because if we
                // put it in ours, it messes with Surge's tab-focus ordering for
                // accessibility. It was originally placed in the main frame
                // before we moved everything to its own component, so keep
                // doing that.
                editor_->addAndMakeVisibleWithTracking(editor_->frame.get(), *labels_[i]);
            }
            else
            {
                labels_[i].reset(nullptr);
            }

            // Finally the FX parameters themselves.
            if (fx.p[i].ctrltype != ct_none)
            {
                makeParameter(paramToParamIndex[i]);
            }
        }
    }

    // Explicitly turn off the VUs, if they were on in a previous layout.
    std::fill(std::begin(vus), std::end(vus), nullptr);
}

void CurrentFxDisplay::conditionerLayout()
{
    // For conditioner, default layout and then add the three VUs.
    defaultLayout();
    auto label_rect = fxRect();
    ParamConfig::VUType t;
    int ypos;
    for (int i = 0; i < 3; i++)
    {
        if (!vus[i])
            vus[i] = std::make_unique<Surge::Widgets::VuMeter>();
        auto vr = label_rect.translated(6, yofs * ConditionerEffect::vu_ypos(i)).translated(0, -14);
        vus[i]->setBounds(vr);
        vus[i]->setSkin(editor_->currentSkin, editor_->bitmapStore);
        vus[i]->setType(ConditionerEffect::vu_type(i));

        editor_->addAndMakeVisibleWithTracking(this, *vus[i]);
    }
}

void CurrentFxDisplay::convolutionLayout()
{
    ConvolutionEffect &fx = dynamic_cast<ConvolutionEffect &>(*editor_->synth->fx[current_fx_]);

    auto vr = fxRect().withTrimmedTop(-1)
        .withTrimmedRight(-5)
        .translated(5, -12)
        .translated(0, yofs * 2);

    auto button = std::make_unique<ConvolutionButton>();
    button->setStorage(storage);
    button->setSurgeGUIEditor(editor_);
    button->setEffect(&fx, &storage->getPatch().fx[current_fx_]);
    button->setBounds(vr);
    auto ol = std::make_unique<OverlayAsAccessibleButton<ConvolutionButton>>(
        button.get(), "No IR loaded", juce::AccessibilityRole::button);
    irbutton = std::move(button);
    addAndMakeVisible(*irbutton);

    std::string irname("No IR loaded.");
    if (fx.initialized)
    {
        irname = storage->getPatch().fx[current_fx_].by_key("irname").to_string();
    }
    ol->setTitle(irname);
    ol->setDescription(irname);

    ol->onPress = [this](ConvolutionButton *c) { showConvolutionMenu(); };
    ol->onMenuKey = [this](ConvolutionButton *c) {
        showConvolutionMenu();
        return true;
    };

    menu = std::move(ol);
    menu->setBounds(vr);
    addAndMakeVisible(*menu);

    defaultLayout();
}

void CurrentFxDisplay::conditionerRender()
{
    if (!editor_->synth->fx[current_fx_])
        return;

    // Set the current VUs to their values.
    const ConditionerEffect &fx =
        dynamic_cast<ConditionerEffect &>(*editor_->synth->fx[current_fx_]);
    for (int i = 0; i < 3; i++)
    {
        // Ensure the layout has actually happened by this point.
        if (!vus[i])
            continue;

        vus[i]->setValue(fx.vu[i][0]);
        vus[i]->setValueR(fx.vu[i][1]);
        vus[i]->repaint();
    }
}

void CurrentFxDisplay::showConvolutionMenu()
{
    std::cout << "In showConvolutionMenu()" << std::endl;
}

void CurrentFxDisplay::vocoderLayout()
{
    // For vocoder, the only change is in label #0.
    defaultLayout();
    labels_[0]->setLabel(fmt::format("Input delayed {} samples", BLOCK_SIZE));
}

void CurrentFxDisplay::layoutFxSelector()
{
    auto currentSkin = editor_->currentSkin;
    auto conn = Surge::Skin::Connector::connectorByID("fx.selector");
    auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);

    // FIXOWN
    editor_->componentForSkinSessionOwnedByMember(skinCtrl->sessionid, editor_->effectChooser);

    editor_->effectChooser->addListener(editor_);
    editor_->effectChooser->setStorage(&(editor_->synth->storage));
    editor_->effectChooser->setBounds(skinCtrl->getRect());
    editor_->effectChooser->setTag(tag_fx_select);
    editor_->effectChooser->setSkin(currentSkin, editor_->bitmapStore);
    editor_->effectChooser->setBackgroundDrawable(editor_->bitmapStore->getImage(IDB_FX_GRID));
    editor_->effectChooser->setCurrentEffect(current_fx_);

    for (int fxi = 0; fxi < n_fx_slots; fxi++)
    {
        editor_->effectChooser->setEffectType(
            fxi, editor_->synth->storage.getPatch().fx[fxi].type.val.i);
    }

    editor_->effectChooser->setBypass(editor_->synth->storage.getPatch().fx_bypass.val.i);
    editor_->effectChooser->setDeactivatedBitmask(
        editor_->synth->storage.getPatch().fx_disable.val.i);

    editor_->addAndMakeVisibleWithTracking(this, *editor_->effectChooser);

    editor_->setAccessibilityInformationByTitleAndAction(editor_->effectChooser->asJuceComponent(),
                                                         "FX Slots", "Select");
}

void CurrentFxDisplay::layoutFxPresetLabel()
{
    auto currentSkin = editor_->currentSkin;
    auto conn = Surge::Skin::Connector::connectorByID("fx.preset.name");
    auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);

    if (!editor_->fxPresetLabel)
    {
        editor_->fxPresetLabel = std::make_unique<juce::Label>("FX Preset Name");
    }

    editor_->fxPresetLabel->setColour(juce::Label::textColourId,
                                      editor_->currentSkin->getColor(Colors::Effect::Preset::Name));
    editor_->fxPresetLabel->setFont(editor_->currentSkin->fontManager->displayFont);
    editor_->fxPresetLabel->setJustificationType(juce::Justification::centredRight);

    editor_->fxPresetLabel->setText(editor_->fxPresetName[current_fx_], juce::dontSendNotification);
    editor_->fxPresetLabel->setBounds(skinCtrl->getRect());
    editor_->setAccessibilityInformationByTitleAndAction(editor_->fxPresetLabel.get(), "FX Preset",
                                                         "Show");

    editor_->addAndMakeVisibleWithTracking(this, *editor_->fxPresetLabel);
}

void CurrentFxDisplay::layoutJogFx()
{
    auto currentSkin = editor_->currentSkin;
    auto conn = Surge::Skin::Connector::connectorByID("fx.preset.prevnext");
    auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);

    // This one's a little weird -- it's just a multiswitch, which is set up
    // by default in layoutComponentForSkin for the tag. Since it's set up
    // there, they own the component.
    auto q = editor_->layoutComponentForSkin(skinCtrl, tag_mp_jogfx);
    editor_->setAccessibilityInformationByTitleAndAction(q->asJuceComponent(), "FX Preset", "Jog");
}

juce::Rectangle<int> CurrentFxDisplay::fxRect()
{
    auto fxpp = editor_->currentSkin->getOrCreateControlForConnector("fx.param.panel");
    return juce::Rectangle<int>(fxpp->x, fxpp->y, 123, 13);
}

bool CurrentFxDisplay::loadWavForConvolution(const juce::String &file)
{
    juce::WavAudioFormat format;
    std::unique_ptr<juce::MemoryMappedAudioFormatReader> reader(
        format.createMemoryMappedReader(file));
    if (!reader)
        return false;
    if (reader->numChannels != 1 && reader->numChannels != 2)
        return false;
    if (!reader->mapEntireFile())
        return false;

    juce::AudioBuffer<float> buf(reader->numChannels, reader->lengthInSamples);
    if (!reader->read(&buf, 0, reader->lengthInSamples, 0, true, true))
        return false;

    FxStorage &sync = editor_->synth->fxsync[current_fx_];

    // Convolution only uses up to the first three ArbitraryBlockStorage.
    std::string name = juce::File(file).getFileNameWithoutExtension().toStdString();
    constexpr int sz = sizeof(float);
    sync.user_data.clear();

    // Filename
    sync.user_data.emplace("irname", ArbitraryBlockStorage::from_string(name));

    // Sample rate.
    sync.user_data.emplace("samplerate", ArbitraryBlockStorage::from_float(reader->sampleRate));

    // Left channel (or mono channel, if it's the only one)
    sync.user_data.emplace("left", ArbitraryBlockStorage::from_floats(std::span<const float>(
                                       buf.getReadPointer(0), reader->lengthInSamples)));

    // Right channel, if it exists.
    if (reader->numChannels == 2)
    {
        sync.user_data.emplace("right", ArbitraryBlockStorage::from_floats(std::span<const float>(
                                            buf.getReadPointer(1), reader->lengthInSamples)));
    }

    // Copy existing parameters to the reload parameter storage, so when we
    // reload everything doesn't jump.
    sync.type = storage->getPatch().fx[current_fx_].type;
    sync.return_level = storage->getPatch().fx[current_fx_].return_level;
    for (std::size_t i = 0; i < n_fx_params; i++)
    {
        sync.p[i] = storage->getPatch().fx[current_fx_].p[i];
    }
    editor_->synth->fx_reload[current_fx_] = true;
    editor_->synth->load_fx_needed = true;
    return true;
}

// File drag and drop for convolution reverb.
bool CurrentFxDisplay::canDropTarget(const juce::String &file)
{
    switch (storage->getPatch().fx[current_fx_].type.val.i)
    {
    case fxt_convolution:
        return file.endsWith(".wav");
    default:
        return false;
    }
}

void CurrentFxDisplay::onDrop(const juce::String &file)
{
    switch (storage->getPatch().fx[current_fx_].type.val.i)
    {
    case fxt_convolution:
        if (file.endsWith(".wav"))
            if (!loadWavForConvolution(file))
                std::cout << "Failed to load IR from " << file << std::endl;
        break;
    }
}

} // namespace Widgets
} // namespace Surge

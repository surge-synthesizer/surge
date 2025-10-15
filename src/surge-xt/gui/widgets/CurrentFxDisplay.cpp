#include <algorithm>
#include "CurrentFxDisplay.h"
#include "EffectChooser.h"
#include "RuntimeFont.h"
#include "SkinModel.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUIEditorTags.h"
#include "dsp/effects/ConditionerEffect.h"

namespace
{
const int yofs = 10;
} // namespace

namespace Surge
{
namespace Widgets
{

CurrentFxDisplay::CurrentFxDisplay(SurgeGUIEditor *e)
{
    editor_ = e;
    storage_ = e->getStorage();
}

CurrentFxDisplay::~CurrentFxDisplay() {}

void CurrentFxDisplay::renderCurrentFx()
{
    switch (storage_->getPatch().fx[current_fx_].type.val.i)
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

    switch (storage_->getPatch().fx[current_fx].type.val.i)
    {
    case fxt_vocoder:
        vocoderLayout();
        break;
    case fxt_conditioner:
        conditionerLayout();
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
    FxStorage &fx = storage_->getPatch().fx[current_fx_];

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

    // First the FX Selector, type, preset, and jog.
    layoutFxSelector();
    // This one's a little weird: the construction is done in layoutComponentForSkin.
    makeParameter(fx_type);
    layoutFxPresetLabel();
    layoutJogFx();

    // Now the FX params.
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

                editor_->addAndMakeVisibleWithTracking(this, *labels_[i]);
            }
            else
            {
                labels_[i].reset(nullptr);
            }

            // Now the FX proper.
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

    editor_->addAndMakeVisibleWithTracking(editor_->frame->getControlGroupLayer(cg_FX),
                                           *editor_->effectChooser);

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

} // namespace Widgets
} // namespace Surge

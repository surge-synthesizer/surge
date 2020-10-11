#include "SkinColors.h"

using namespace VSTGUI;

namespace Colors
{
   namespace AboutBox
   {
      const Surge::UI::SkinColor Text("aboutbox.text", kWhiteCColor),
                                 Link("aboutbox.link", CColor(46, 134, 255));
   }

   namespace Dialog
   {
      const Surge::UI::SkinColor Background("dialog.background", CColor(205, 206, 212)),
                                 Border("dialog.border", CColor(160, 164, 183));

      namespace Titlebar
      {
         const Surge::UI::SkinColor Background("dialog.titlebar.background", CColor(160, 164, 183)),
                                    Text("dialog.titlebar.text", kWhiteCColor);
      }
      namespace Button
      {
         const Surge::UI::SkinColor Background("dialog.button.background", CColor(227, 227, 227)),
                                    Border("dialog.button.border", CColor(160, 160, 160)),
                                    Text("dialog.button.text", kBlackCColor),
                                    BackgroundHover("dialog.button.hover.background", CColor(255, 144, 0)),
                                    BorderHover("dialog.button.hover.border", CColor(160, 160, 160)),
                                    TextHover("dialog.button.hover.text", kWhiteCColor);
      }
      namespace Checkbox
      {
         const Surge::UI::SkinColor Background("dialog.checkbox.background", kWhiteCColor),
                                    Border("dialog.checkbox.border", kBlackCColor),
                                    Tick("dialog.checkbox.tick", kBlackCColor);
      }
      namespace Entry
      {
         const Surge::UI::SkinColor Background("dialog.textfield.background", kWhiteCColor),
                                    Border("dialog.textfield.border", CColor(160, 160, 160)),
                                    Focus("dialog.textfield.focus", CColor(170, 170, 230)),
                                    Text("dialog.textfield.text", kBlackCColor);
      }
      namespace Label
      {
         const Surge::UI::SkinColor Error("dialog.label.text.error", kRedCColor),
                                    Text("dialog.label.text", kBlackCColor);
      }
   }

   namespace Effect
   {
      namespace Label
      {
         const Surge::UI::SkinColor Text("effect.label.text", CColor(76, 76, 76)),
                                    Separator("effect.label.separator", CColor(106, 106, 106));
      }
      namespace Preset
      {
         const Surge::UI::SkinColor Name("effect.label.presetname", kBlackCColor);
      }
      namespace Menu
      {
         const Surge::UI::SkinColor Text("fxmenu.text", kBlackCColor);
      }
      namespace SelectorPanel
      {
         const Surge::UI::SkinColor LabelBorder( "effect.selectorpanel.labelborder", kBlackCColor );
      }
   }

   namespace InfoWindow
   {
      const Surge::UI::SkinColor Background("infowindow.background", kWhiteCColor);
      const Surge::UI::SkinColor Border("infowindow.border", kBlackCColor);
      const Surge::UI::SkinColor Text("infowindow.text", kBlackCColor);

      namespace Modulation
      {
         const Surge::UI::SkinColor Negative("infowindow.text.modulation.negative", kBlackCColor),
                                    Positive("infowindow.text.modulation.positive", kBlackCColor),
                                    ValueNegative("infowindow.text.modulation.value.negative", kBlackCColor),
                                    ValuePositive("infowindow.text.modulation.value.positive", kBlackCColor);
      }
   }

   namespace LFO
   {
      namespace Title
      {
          const Surge::UI::SkinColor Text("lfo.title.text", CColor(0, 0, 0, 192));
      }

      namespace Type
      {
         const Surge::UI::SkinColor Text("lfo.type.unselected.text", kBlackCColor),
                                    SelectedText("lfo.type.selected.text", kBlackCColor),
                                    SelectedBackground("lfo.type.selected.background", CColor(255, 152, 21));
      }
      namespace StepSeq
      {
         const Surge::UI::SkinColor Background("lfo.stepseq.background", CColor(255, 144, 0)),
                                    ColumnShadow("lfo.stepseq.column.shadow", CColor(109, 109, 125)),
                                    DragLine("lfo.stepseq.drag.line", CColor(221, 221, 255)),
                                    Envelope("lfo.stepseq.envelope", CColor(109, 109, 125)),
                                    TriggerClick("lfo.stepseq.trigger.click", CColor(50, 84, 131)),
                                    Wave("lfo.stepseq.wave", kWhiteCColor);

         namespace Button
         {
            const Surge::UI::SkinColor Background("lfo.stepseq.button.background", CColor(151, 152, 154)),
                                       Border("lfo.stepseq.button.border", CColor(93, 93, 93)),
                                       Hover("lfo.stepseq.button.hover", CColor(174, 175, 190)),
                                       Arrow("lfo.stepseq.button.arrow", kWhiteCColor);
         }
         namespace InfoWindow
         {
            const Surge::UI::SkinColor Background("lfo.stepseq.popup.background", kWhiteCColor),
                                       Border("lfo.stepseq.popup.border", kBlackCColor),
                                       Text("lfo.stepseq.popup.text", kBlackCColor);
         }
         namespace Loop
         {
            const Surge::UI::SkinColor Marker("lfo.stepseq.loop.markers", CColor(18, 52, 99)),
                                       PrimaryStep("lfo.stepseq.loop.step.primary", CColor(169, 208, 239)),
                                       SecondaryStep("lfo.stepseq.loop.step.secondary", CColor(154, 191, 224)),
                                       OutsidePrimaryStep("lfo.stepseq.loop.outside.step.primary", CColor(222, 222, 222)),
                                       OutsideSecondaryStep("lfo.stepseq.loop.outside.step.secondary", CColor(208, 208, 208));
         }
         namespace Step
         {
            const Surge::UI::SkinColor Fill("lfo.stepseq.step.fill", CColor(18, 52, 99)),
                                       FillDeactivated("lfo.stepseq.step.fill.deactivated", CColor(46, 134, 255)),
                                       FillOutside("lfo.stepseq.step.fill.outside", kGreyCColor);
         }
      }
      namespace Waveform
      {
         const Surge::UI::SkinColor Background("lfo.waveform.background", CColor(255, 144, 0)),
                                    Bounds("lfo.waveform.bounds", CColor(224, 128, 0)),
                                    Center("lfo.waveform.center", CColor(224, 128, 0)),
                                    Envelope("lfo.waveform.envelope", CColor(176, 96, 0)),
                                    Wave("lfo.waveform.wave", kBlackCColor),
                                    DeactivatedWave("lfo.waveform.wave.deactivated", CColor(0, 0, 0, 144));

         namespace Ruler
         {
            const Surge::UI::SkinColor Text("lfo.waveform.ruler.text", kBlackCColor),
                                       Ticks("lfo.waveform.ruler.ticks", kBlackCColor),
                                       ExtendedTicks("lfo.waveform.ruler.ticks.extended", CColor(224, 128, 0)),
                                       SmallTicks("lfo.waveform.ruler.ticks.small", CColor(176, 96, 0));
         }
      }
   }

   namespace Menu
   {
      const Surge::UI::SkinColor Name("menu.name", kBlackCColor),
                                 NameHover("menu.name.hover", CColor(60, 20, 0)),
                                 NameDeactivated("menu.name.deactivated", CColor(180, 180, 180)),
                                 Value("menu.value", kBlackCColor),
                                 ValueHover("menu.value.hover", CColor(60, 20, 0)),
                                 ValueDeactivated("menu.value.deactivated", CColor(180, 180, 180));

      const Surge::UI::SkinColor FilterValue("filtermenu.value", CColor(255, 154, 16)),
                                 FilterValueHover("filtermenu.value.hover", kWhiteCColor);
   }

   namespace ModSource
   {
      namespace Unused
      {
         const Surge::UI::SkinColor Background("modbutton.unused.fill", CColor(18, 52, 99)),
                                    Border("modbutton.unused.frame", CColor(32, 93, 176)),
                                    BorderHover("modbutton.unused.frame.hover", CColor(144, 216, 255)),
                                    Text("modbutton.unused.text", CColor(46, 134, 254)),
                                    TextHover("modbutton.unused.text.hover", CColor(144, 216, 255));
      }
      namespace Used
      {
         const Surge::UI::SkinColor Background("modbutton.used.fill", CColor(18, 52, 99)),
                                    Border("modbutton.used.frame", CColor(64, 173, 255)),
                                    BorderHover("modbutton.used.frame.hover", CColor(192, 235, 255)),
                                    Text("modbutton.used.text", CColor(64, 173, 255)),
                                    TextHover("modbutton.used.text.hover", CColor(192, 235, 255)),
                                    UsedModHover("modbutton.used.text.usedmod.hover", CColor(154, 225, 95));
      }
      namespace Selected
      {
         const Surge::UI::SkinColor Background("modbutton.selected.fill", CColor(35, 100, 192)),
                                    Border("modbutton.selected.frame", CColor(46, 134, 254)),
                                    BorderHover("modbutton.selected.frame.hover", CColor(192, 235, 255)),
                                    Text("modbutton.selected.text", CColor(18, 52, 99)),
                                    TextHover("modbutton.selected.text.hover", CColor(192, 235, 255)),
                                    UsedModHover("modbutton.selected.text.usedmod.hover", CColor(154, 225, 95));
         namespace Used
         {
            const Surge::UI::SkinColor Background("modbutton.selected.used.fill", CColor(35, 100, 192)),
                                       Border("modbutton.selected.used.frame", CColor(144, 216, 255)),
                                       BorderHover("modbutton.selected.used.frame.hover", CColor(185, 255, 255)),
                                       Text("modbutton.selected.used.text", CColor(144, 216, 255)),
                                       TextHover("modbutton.selected.used.text.hover", CColor(185, 255, 255)),
                                       UsedModHover("modbutton.selected.used.text.usedmod.hover", CColor(154, 225, 95));
         }
      }
      namespace Armed
      {
         const Surge::UI::SkinColor Background("modbutton.armed.fill", CColor(116, 172, 72)),
                                    Border("modbutton.armed.frame", CColor(173, 255, 107)),
                                    BorderHover("modbutton.armed.frame.hover", CColor(208, 255, 255)),
                                    Text("modbutton.armed.text", CColor(173, 255, 107)),
                                    TextHover("modbutton.armed.text.hover", CColor(208, 255, 255)),
                                    UsedModHover("modbutton.armed.text.usedmod.hover", CColor(46, 134, 254));
      }
   }

   namespace MSEGEditor
   {
      const Surge::UI::SkinColor Background("msegeditor.background", CColor(17, 17, 17)),
                                 Curve("msegeditor.curve", kWhiteCColor),
                                 Panel("msegeditor.panel", CColor(205, 206, 212)),
                                 Text("msegeditor.panel.text", kBlackCColor);

      namespace Axis
      {
         const Surge::UI::SkinColor Line("msegeditor.axis.line", CColor(220, 220, 240)),
                                    Text("msegeditor.axis.text", kWhiteCColor);
      }
      namespace GradientFill
      {
         const Surge::UI::SkinColor StartColor("msegeditor.fill.gradient.start", CColor(255, 144, 0, 128)),
                                    EndColor("msegeditor.fill.gradient.end", CColor(255, 144, 0, 16));
      }
      namespace Grid
      {
         const Surge::UI::SkinColor Primary("msegeditor.grid.primary", CColor(220, 220, 240)),
                                    Secondary("msegeditor.grid.secondary", CColor(100, 100, 110));
      }
      namespace Loop
      {
         const Surge::UI::SkinColor Line("msegeditor.loop.line", CColor(255, 144, 0));
      }
      namespace NumberField
      {
         const Surge::UI::SkinColor Background("msegeditor.numberfield.background", CColor(216, 216, 216)),
                                    Text("msegeditor.numberfield.text", kBlackCColor),
                                    Border("msegeditor.numberfield.border", CColor(151, 151, 151));
      }
   }

   namespace NumberField
   {
      const Surge::UI::SkinColor Background("numberfield.background", kWhiteCColor),
                                 Text("numberfield.text", CColor(42, 42, 42)),
                                 Border("numberfield.border", CColor(42, 42, 42)),
                                 Env("numberfield.env", CColor(214, 209, 198));
   }

   namespace Osc
   {
      namespace Display
      {
         const Surge::UI::SkinColor Bounds("osc.line.bounds", CColor(70, 70, 70)),
                                    Center("osc.line.center", CColor(90, 90, 90)),
                                    AnimatedWave("osc.waveform.animated", kWhiteCColor),
                                    Wave("osc.waveform", CColor(255, 144, 0));
      }
      namespace Filename
      {
         const Surge::UI::SkinColor Background("osc.wavename.background", CColor(255, 160, 16)),
                                    Text("osc.wavename.text", kBlackCColor);
      }
   }

   namespace Overlay
   {
      const Surge::UI::SkinColor Background("overlay.background", CColor(0, 0, 0, 204));
   }

   namespace PatchBrowser
   {
      const Surge::UI::SkinColor Background("patchbrowser.background", kWhiteCColor),
                                 Text("patchbrowser.text", kBlackCColor);
   }

   namespace Slider
   {
      namespace Label
      {
         const Surge::UI::SkinColor Light("slider.light.label", kWhiteCColor),
                                    Dark("slider.dark.label", kBlackCColor);
      }
      namespace Modulation
      {
         const Surge::UI::SkinColor Positive("slider.modulation.positive", CColor(173, 255, 107)),
                                    Negative("slider.modulation.negative", CColor(173, 255, 107));
      }
   }

   namespace StatusButton
   {
      const Surge::UI::SkinColor Background("status.button.background", CColor(227, 227, 227)),
                                 SelectedBackground("status.button.highlight", CColor(255, 154, 16)),
                                 Border("status.button.border", CColor(151, 151, 151)),
                                 Text("status.button.text.unselected", kBlackCColor),
                                 SelectedText("status.button.text.selected", kBlackCColor);
   }

   namespace VuMeter
   {
      const Surge::UI::SkinColor Level("vumeter.level", CColor(5, 201, 13)),
                                 HighLevel("vumeter.highlevel", kRedCColor),
                                 Background("vumeter.background", CColor(205, 206, 212)),
                                 Border("vumeter.border", kBlackCColor);
   }
}

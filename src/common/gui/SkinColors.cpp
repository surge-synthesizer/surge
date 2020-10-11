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
                                    Text("dialog.titlebar.font", kWhiteCColor);
      }
      namespace Button
      {
         const Surge::UI::SkinColor Background("dialog.button.background", CColor(227, 227, 227)),
                                    Border("dialog.button.border", CColor(160, 160, 160)),
                                    Text("dialog.button.font", kBlackCColor),
                                    BackgroundHover("dialog.button.hover.background", CColor(255, 144, 0)),
                                    BorderHover("dialog.button.hover.border", CColor(160, 160, 160)),
                                    TextHover("dialog.button.hover.font", kWhiteCColor);
      }
      namespace Checkbox
      {
         const Surge::UI::SkinColor Background("dialog.checkbox.fill", kWhiteCColor),
                                    Border("dialog.checkbox.border", kBlackCColor),
                                    Tick("dialog.checkbox.tick", kBlackCColor);
      }
      namespace Entry
      {
         const Surge::UI::SkinColor Background("dialog.textfield.background", kWhiteCColor),
                                    Border("dialog.textfield.border", CColor(160, 160, 160)),
                                    Focus("dialog.textfield.focuscolor", CColor(170, 170, 230)),
                                    Text("dialog.textfield.font", kBlackCColor);
      }
      namespace Label
      {
         const Surge::UI::SkinColor Error("dialog.label.error.font", kRedCColor),
                                    Text("dialog.label.font", kBlackCColor);
      }
   }

   namespace Effect
   {
      namespace Label
      {
         const Surge::UI::SkinColor Text("effect.label.font", CColor(76, 76, 76)),
                                    Line("effect.label.hrule", CColor(106, 106, 106));
      }
      namespace Preset
      {
         const Surge::UI::SkinColor Name("effect.label.presetname", kBlackCColor);
      }
      namespace Menu
      {
         const Surge::UI::SkinColor Text("fxmenu.font", kBlackCColor);
      }
   }

   namespace InfoWindow
   {
      const Surge::UI::SkinColor Background("infowindow.background", kWhiteCColor);
      const Surge::UI::SkinColor Border("infowindow.border", kBlackCColor);
      const Surge::UI::SkinColor Text("infowindow.text", kBlackCColor);

      namespace Modulation
      {
         const Surge::UI::SkinColor Negative("infowindow.font.modulationnegative", kBlackCColor),
                                    Positive("infowindow.font.modulationpositive", kBlackCColor),
                                    ValueNegative("infowindow.font.modulationvaluenegative", kBlackCColor),
                                    ValuePositive("infowindow.font.modulationvaluepositive", kBlackCColor);
      }
   }

   namespace LFO
   {
      namespace Title
      {
          const Surge::UI::SkinColor Text("lfo.title.font", CColor(0, 0, 0, 192));
      }

      namespace Type
      {
         const Surge::UI::SkinColor Text("lfo.type.unselected.font", kBlackCColor),
                                    SelectedText("lfo.type.selected.font", kBlackCColor),
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
            const Surge::UI::SkinColor Background("lfo.stepseq.button.fill", CColor(151, 152, 154)),
                                       Border("lfo.stepseq.button.border", CColor(93, 93, 93)),
                                       Hover("lfo.stepseq.button.hover", CColor(174, 175, 190)),
                                       Arrow("lfo.stepseq.button.arrow.fill", kWhiteCColor);
         }
         namespace InfoWindow
         {
            const Surge::UI::SkinColor Background("lfo.stepseq.popup.background", kWhiteCColor),
                                       Border("lfo.stepseq.popup.border", kBlackCColor),
                                       Text("lfo.stepseq.popup.font", kBlackCColor);
         }
         namespace Loop
         {
            const Surge::UI::SkinColor Marker("lfo.stepseq.loop.markers", CColor(18, 52, 99)),
                                       MajorStep("lfo.stepseq.loop.majorstep", CColor(169, 208, 239)),
                                       MinorStep("lfo.stepseq.loop.minorstep", CColor(154, 191, 224)),
                                       OutsideMajorStep("lfo.stepseq.loop.outside.majorstep", CColor(222, 222, 222)),
                                       OutsideMinorStep("lfo.stepseq.loop.outside.minorstep", CColor(208, 208, 208));
         }
         namespace Step
         {
            const Surge::UI::SkinColor Fill("lfo.stepseq.step.fill", CColor(18, 52, 99)),
                                       FillDeactivated("lfo.stepseq.step.fill.deactivated", CColor(46, 134, 255)),
                                       OutsideFill("lfo.stepseq.step.fill.disabled", kWhiteCColor);
         }
      }
      namespace Waveform
      {
         const Surge::UI::SkinColor Background("lfo.waveform.background", CColor(255, 144, 0)),
                                    Bounds("lfo.waveform.bounds", CColor(224, 128, 0)),
                                    CenterLine("lfo.waveform.centerline", CColor(224, 128, 0)),
                                    Envelope("lfo.waveform.envelope", CColor(176, 96, 0)),
                                    Fill("lfo.waveform.fill", CColor(255, 144, 0)),
                                    MajorDivisions("lfo.waveform.majordivisions", CColor(224, 128, 0)),
                                    Wave("lfo.waveform.wave", kBlackCColor),
                                    DeactivatedWave( "lfo.waveform.deactivatedwave", CColor( 176, 96, 0 ) );

         namespace Ruler
         {
            const Surge::UI::SkinColor Text("lfo.waveform.ruler.font", kBlackCColor),
                                       Ticks("lfo.waveform.ruler.ticks", kBlackCColor),
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
                                    Text("modbutton.unused.font", CColor(46, 134, 254)),
                                    TextHover("modbutton.unused.font.hover", CColor(144, 216, 255));
      }
      namespace Used
      {
         const Surge::UI::SkinColor Background("modbutton.used.fill", CColor(18, 52, 99)),
                                    Border("modbutton.used.frame", CColor(64, 173, 255)),
                                    BorderHover("modbutton.used.frame.hover", CColor(192, 235, 255)),
                                    Text("modbutton.used.font", CColor(64, 173, 255)),
                                    TextHover("modbutton.used.font.hover", CColor(192, 235, 255)),
                                    UsedModHover("modbutton.used.font.usedmod.hover", CColor(154, 225, 95));
      }
      namespace Selected
      {
         const Surge::UI::SkinColor Background("modbutton.selected.fill", CColor(35, 100, 192)),
                                    Border("modbutton.selected.frame", CColor(46, 134, 254)),
                                    BorderHover("modbutton.selected.frame.hover", CColor(192, 235, 255)),
                                    Text("modbutton.selected.font", CColor(18, 52, 99)),
                                    TextHover("modbutton.selected.font.hover", CColor(192, 235, 255)),
                                    UsedModHover("modbutton.selected.font.usedmod.hover", CColor(154, 225, 95));
         namespace Used
         {
            const Surge::UI::SkinColor Background("modbutton.selected.used.fill", CColor(35, 100, 192)),
                                       Border("modbutton.selected.used.frame", CColor(144, 216, 255)),
                                       BorderHover("modbutton.selected.used.frame.hover", CColor(185, 255, 255)),
                                       Text("modbutton.selected.used.font", CColor(144, 216, 255)),
                                       TextHover("modbutton.selected.used.font.hover", CColor(185, 255, 255)),
                                       UsedModHover("modbutton.selected.used.font.usedmod.hover", CColor(154, 225, 95));
         }
      }
      namespace Armed
      {
         const Surge::UI::SkinColor Background("modbutton.armed.fill", CColor(116, 172, 72)),
                                    Border("modbutton.armed.frame", CColor(173, 255, 107)),
                                    BorderHover("modbutton.armed.frame.hover", CColor(208, 255, 255)),
                                    Text("modbutton.armed.font", CColor(173, 255, 107)),
                                    TextHover("modbutton.armed.font.hover", CColor(208, 255, 255)),
                                    UsedModHover("modbutton.armed.font.usedmod.hover", CColor(46, 134, 254));
      }
   }

   namespace MSEGEditor
   {
      const Surge::UI::SkinColor Background("msegeditor.background", CColor(17, 17, 17)),
                                 Curve("msegeditor.curve", kWhiteCColor),
                                 Panel("msegeditor.panel", CColor(205, 206, 212)),
                                 Text("msegeditor.panel.font", kBlackCColor);

      namespace Axis
      {
         const Surge::UI::SkinColor Line("msegeditor.axis.line", CColor(220, 220, 240)),
                                    Text("msegeditor.axis.font", kWhiteCColor);
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
   }

   namespace NumberField
   {
      const Surge::UI::SkinColor Background("control.numberfield.background", kWhiteCColor),
                                 Text("control.numberfield.font", CColor(42, 42, 42)),
                                 Border("control.numberfield.rule", CColor(42, 42, 42)),
                                 Env("control.numberfield.env", CColor(214, 209, 198));
   }

   namespace Osc
   {
      namespace Display
      {
         const Surge::UI::SkinColor Bounds("osc.topline", CColor(70, 70, 70)),
                                    CenterLine("osc.midline", CColor(90, 90, 90)),
                                    AnimatedWave("osc.label", kWhiteCColor),
                                    Wave("osc.wave", CColor(255, 144, 0));
      }
      namespace Filename
      {
         const Surge::UI::SkinColor Background("osc.wavename.background", CColor(255, 160, 16)),
                                    Text("osc.wavename.font", kBlackCColor);
      }
   }

   namespace Overlay
   {
      const Surge::UI::SkinColor Background("overlay.background", CColor(0, 0, 0, 204));
   }

   namespace PatchBrowser
   {
      const Surge::UI::SkinColor Background("patchbrowser.background", kWhiteCColor),
                                 Text("patchbrowser.font", kBlackCColor);
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
         const Surge::UI::SkinColor Positive("slider.modulation", CColor(173, 255, 107)),
                                    Negative("slider.negative.modulation", CColor(173, 255, 107));
      }
   }

   namespace StatusButton
   {
      const Surge::UI::SkinColor Background("status.button.background", CColor(227, 227, 227)),
                                 Border("status.button.outline", CColor(151, 151, 151)),
                                 Text("status.button.unselected.font", kBlackCColor),
                                 SelectedBackground("status.button.highlight", CColor(255, 154, 16)),
                                 SelectedText("status.button.selected.font", kBlackCColor);
   }

   namespace VuMeter
   {
      const Surge::UI::SkinColor Level("vumeter.level", CColor(5, 201, 13)),
                                 HighLevel("vumeter.highlevel", kRedCColor),
                                 Background("vumeter.background", CColor(205, 206, 212)),
                                 Border("vumeter.border", kBlackCColor);
   }
}

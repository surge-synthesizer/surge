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
      const Surge::UI::SkinColor Background("savedialog.background", CColor(205, 206, 212)),
                                 Border("savedialog.border", CColor(160, 164, 183));

      namespace Titlebar
      {
         const Surge::UI::SkinColor Background("dialog.titlebar.background", CColor(160, 164, 183)),
                                    Text("dialog.titlebar.textlabel", kWhiteCColor);
      }
      namespace Button
      {
         const Surge::UI::SkinColor Background("dialog.button.background", CColor(227, 227, 227)),
                                    Border("dialog.button.border", CColor(160, 160, 160)),
                                    Text("dialog.button.textlabel", kBlackCColor);
      }
      namespace Checkbox
      {
         const Surge::UI::SkinColor Background("savedialog.checkbox.fill", kWhiteCColor),
                                    Border("savedialog.checkbox.border", kBlackCColor),
                                    Tick("savedialog.checkbox.tick", kBlackCColor);
      }
      namespace Entry
      {
         const Surge::UI::SkinColor Background("savedialog.textfield.background", kWhiteCColor),
                                    Border("savedialog.textfield.border", CColor(160, 160, 160)),
                                    Focus("textfield.focuscolor", CColor(170, 170, 230)),
                                    Text("savedialog.textfield.foreground", kBlackCColor);
      }
      namespace Label
      {
         const Surge::UI::SkinColor Error("savedialog.textlabel.error", kRedCColor),
                                    Text("savedialog.textlabel", kBlackCColor);
      }
   }

   namespace Effect
   {
      namespace Label
      {
         const Surge::UI::SkinColor Text("effect.label.foreground", CColor(76, 76, 76)),
                                    Line("effect.label.hrule", CColor(106, 106, 106));
      }
      namespace Preset
      {
         const Surge::UI::SkinColor Name("effect.label.presetname", kBlackCColor);
      }
      namespace Menu
      {
         const Surge::UI::SkinColor Text("fxmenu.foreground", kBlackCColor);
      }
   }

   namespace InfoWindow
   {
      const Surge::UI::SkinColor Background("infowindow.background", kWhiteCColor);
      const Surge::UI::SkinColor Border("infowindow.border", kBlackCColor);
      const Surge::UI::SkinColor Text("infowindow.text", kBlackCColor);

      namespace Modulation
      {
         const Surge::UI::SkinColor Negative("infowindow.foreground.modulationnegative", kBlackCColor),
                                    Positive("infowindow.foreground.modulationpositive", kBlackCColor),
                                    ValueNegative("infowindow.foreground.modulationvaluenegative", kBlackCColor),
                                    ValuePositive("infowindow.foreground.modulationvaluepositive", kBlackCColor);
      }
   }

   namespace LFO
   {
      namespace Title
      {
          const Surge::UI::SkinColor Text("lfo.title.foreground", CColor(0, 0, 0, 192));
      }

      namespace Type
      {
         const Surge::UI::SkinColor Text("lfo.type.unselected.foreground", kBlackCColor),
                                    SelectedText("lfo.type.selected.foreground", kBlackCColor),
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
                                       Text("lfo.stepseq.popup.foreground", kBlackCColor);
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
                                    Wave("lfo.waveform.wave", kBlackCColor);

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
      const Surge::UI::SkinColor Name("menu.name"),
                                 NameHover("menu.name.hover"),
                                 NameDeactivated("menu.name.deactivated"),
                                 Value("menu.value"),
                                 ValueHover("menu.value.hover"),
                                 ValueDeactivated("menu.value.deactivated");

      const Surge::UI::SkinColor FilterValue("filtermenu.value"),
                                 FilterValueHover("filtermenu.value.hover");
   }

   namespace ModSource
   {
      namespace Unused
      {
         const Surge::UI::SkinColor Background("modbutton.unused.fill"),
                                    Border("modbutton.unused.frame"),
                                    BorderHover("modbutton.unused.frame.hover"),
                                    Text("modbutton.unused.font"),
                                    TextHover("modbutton.unused.font.hover");
      }
      namespace Used
      {
         const Surge::UI::SkinColor Background("modbutton.used.fill"),
                                    Border("modbutton.used.frame"),
                                    BorderHover("modbutton.used.frame.hover"),
                                    Text("modbutton.used.font"),
                                    TextHover("modbutton.used.font.hover"),
                                    UsedHover("modbutton.used.font.usedmod.hover");
      }
      namespace Selected
      {
         const Surge::UI::SkinColor Background("modbutton.selected.fill"),
                                    Border("modbutton.selected.frame"),
                                    BorderHover("modbutton.selected.frame.hover"),
                                    Text("modbutton.selected.font"),
                                    TextHover("modbutton.selected.font.hover"),
                                    UsedHover("modbutton.selected.font.usedmod.hover");
      }
      namespace Armed
      {
         const Surge::UI::SkinColor Background("modbutton.armed.fill"),
                                    Border("modbutton.armed.frame"),
                                    BorderHover("modbutton.armed.frame.hover"),
                                    Text("modbutton.armed.font"),
                                    TextHover("modbutton.armed.font.hover"),
                                    UsedHover("modbutton.armed.font.usedmod.hover");
      }
   }

   namespace MSEGEditor
   {
      const Surge::UI::SkinColor Line("msegeditor.line");

      namespace Axis
      {
         const Surge::UI::SkinColor Line("msegeditor.axis.line");
      }
      namespace Grid
      {
         const Surge::UI::SkinColor Primary("msegeditor.grid.primary"),
                                    Secondary("msegeditor.grid.usedmod");
      }
      namespace Loop
      {
         const Surge::UI::SkinColor Line("msegeditor.loop.line");
      }
      namespace Segment
      {
         const Surge::UI::SkinColor Hover("msegeditor.segment.hover");
      }
   }

   namespace NumberField
   {
      const Surge::UI::SkinColor Background("control.numberfield.background"),
                                 Text("control.numberfield.foreground"),
                                 Border("control.numberfield.rule"),
                                 Env("control.numberfield.env");
   }

   namespace Osc
   {
      namespace Display
      {
         const Surge::UI::SkinColor Bounds("osc.topline"),
                                    CenterLine("osc.midline"),
                                    AnimatedWave("osc.label"),
                                    Wave("osc.wave");
      }
      namespace Filename
      {
         const Surge::UI::SkinColor Background("osc.wavename.background"),
                                    Text("osc.wavename.foreground");
      }
   }

   namespace Overlay
   {
      const Surge::UI::SkinColor Background("overlay.background"),
                                 Border("overlay.border");

      namespace Container
      {
         const Surge::UI::SkinColor Background("overlay.container.background"),
                                    TitleText("overlay.container.title");
      }
   }

   namespace PatchBrowser
   {
      const Surge::UI::SkinColor Background("patchbrowser.background"),
                                 Text("patchbrowser.foreground");
   }

   namespace Slider
   {
      namespace Label
      {
         const Surge::UI::SkinColor Light("slider.light.label"),
                                    Dark("slider.dark.label");
      }
      namespace Modulation
      {
         const Surge::UI::SkinColor Positive("slider.modulation"),
                                    Negative("slider.negative.modulation");
      }
   }

   namespace StatusButton
   {
      const Surge::UI::SkinColor Background("status.button.background"),
                                 Border("status.button.outline"),
                                 Text("status.button.unselected.foreground"),
                                 SelectedBackground("status.button.highlight"),
                                 SelectedText("status.button.selected.foreground");
   }

   namespace VuMeter
   {
      const Surge::UI::SkinColor Level("vumeter.level"),
                                 HighLevel("vumeter.highlevel"),
                                 Background("vumeter.background"),
                                 Border("vumeter.border");
   }
}

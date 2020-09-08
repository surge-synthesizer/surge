#include "SkinColors.h"

namespace Colors
{
   namespace AboutBox
   {
      const Surge::UI::SkinColor Text("aboutbox.text"),
                                 Link("aboutbox.link");
   }

   namespace Dialog
   {
      const Surge::UI::SkinColor Background("savedialog.background"),
                                 Border("savedialog.border");

      namespace Checkbox
      {
         const Surge::UI::SkinColor Background("savedialog.checkbox.fill"),
                                    Border("savedialog.checkbox.border"),
                                    Tick("savedialog.checkbox.tick");
      }
      namespace Entry
      {
         const Surge::UI::SkinColor Background("savedialog.textfield.background"),
                                    Border("savedialog.textfield.border"),
                                    Focus("textfield.focuscolor"),
                                    Text("savedialog.textfield.foreground");
      }
      namespace Label
      {
         const Surge::UI::SkinColor Error("savedialog.textlabel.error"),
                                    Text("savedialog.textlabel");
      }
   }

   namespace Effect
   {
      namespace Label
      {
         const Surge::UI::SkinColor Text("effect.label.foreground"),
                                    Line("effect.label.hrule");
      }
      namespace Preset
      {
         const Surge::UI::SkinColor Name("effect.label.presetname");
      }
      namespace Menu
      {
         const Surge::UI::SkinColor Text("fxmenu.foreground");
      }
   }

   namespace InfoWindow
   {
      const Surge::UI::SkinColor Background("infowindow.background");
      const Surge::UI::SkinColor Border("infowindow.border");
      const Surge::UI::SkinColor Text("infowindow.text");

      namespace Modulation
      {
         const Surge::UI::SkinColor Negative("infowindow.foreground.modulationnegative"),
                                    Positive("infowindow.foreground.modulationpositive"),
                                    ValueNegative("infowindow.foreground.modulationvaluenegative"),
                                    ValuePositive("infowindow.foreground.modulationvaluepositive");
      }
   }

   namespace LFO
   {
      namespace Type
      {
         const Surge::UI::SkinColor Text("lfo.type.unselected.foreground"),
                                    SelectedText("lfo.type.selected.foreground"),
                                    SelectedBackground("lfo.type.selected.background");
      }
      namespace StepSeq
      {
         const Surge::UI::SkinColor Background("lfo.stepseq.background"),
                                    ColumnShadow("lfo.stepseq.column.shadow"),
                                    DragLine("lfo.stepseq.drag.line"),
                                    Envelope("lfo.stepseq.envelope"),
                                    TriggerClick("lfo.stepseq.trigger.click"),
                                    Wave("lfo.stepseq.wave");

         namespace Button
         {
            const Surge::UI::SkinColor Background("lfo.stepseq.button.fill"),
                                       Border("lfo.stepseq.button.border"),
                                       Hover("lfo.stepseq.button.hover"),
                                       Arrow("lfo.stepseq.button.arrow.fill");
         }
         namespace InfoWindow
         {
            const Surge::UI::SkinColor Background("lfo.stepseq.popup.background"),
                                       Border("lfo.stepseq.popup.border"),
                                       Text("lfo.stepseq.popup.foreground");
         }
         namespace Loop
         {
            const Surge::UI::SkinColor Marker("lfo.stepseq.loop.markers"),
                                       MajorStep("lfo.stepseq.loop.majorstep"),
                                       MinorStep("lfo.stepseq.loop.minorstep"),
                                       OutsideMajorStep("lfo.stepseq.loop.outside.majorstep"),
                                       OutsideMinorStep("lfo.stepseq.loop.outside.minorstep");
         }
         namespace Step
         {
            const Surge::UI::SkinColor Fill("lfo.stepseq.step.fill"),
                                       FillForDeactivatedRate("lfo.stepseq.step.fill.deactivatedrate"),
                                       OutsideFill("lfo.stepseq.step.fill.disabled");
         }
      }
      namespace Waveform
      {
         const Surge::UI::SkinColor Background("lfo.waveform.background"),
                                    Bounds("lfo.waveform.bounds"),
                                    CenterLine("lfo.waveform.centerline"),
                                    Envelope("lfo.waveform.envelope"),
                                    Fill("lfo.waveform.fill"),
                                    MajorDivisions("lfo.waveform.majordivisions"),
                                    Wave("lfo.waveform.wave");

         namespace Ruler
         {
            const Surge::UI::SkinColor Text("lfo.waveform.ruler.font"),
                                       Ticks("lfo.waveform.ruler.ticks");
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
      namespace Active
      {
         const Surge::UI::SkinColor Background("modbutton.active.fill"),
                                    Border("modbutton.active.frame"),
                                    BorderHover("modbutton.active.frame.hover"),
                                    Text("modbutton.active.font"),
                                    TextHover("modbutton.active.font.hover"),
                                    UsedHover("modbutton.active.secondary.hover");
      }
      namespace Inactive
      {
         const Surge::UI::SkinColor Background("modbutton.inactive.fill"),
                                    Border("modbutton.inactive.frame"),
                                    BorderHover("modbutton.inactive.frame.hover"),
                                    Text("modbutton.inactive.font"),
                                    TextHover("modbutton.inactive.font.hover"),
                                    UsedHover("modbutton.inactive.secondary.hover");
      }
      namespace Selected
      {
         const Surge::UI::SkinColor Background("modbutton.selected.fill"),
                                    Border("modbutton.selected.frame"),
                                    BorderHover("modbutton.selected.frame.hover"),
                                    Text("modbutton.selected.font"),
                                    TextHover("modbutton.selected.font.hover"),
                                    UsedHover("modbutton.selected.secondary.hover");
      }
      namespace Used
      {
         const Surge::UI::SkinColor Background("modbutton.used.fill"),
                                    Border("modbutton.used.frame"),
                                    BorderHover("modbutton.used.frame.hover"),
                                    Text("modbutton.used.font"),
                                    TextHover("modbutton.used.font.hover");
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
                                    Secondary("msegeditor.grid.secondary");
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

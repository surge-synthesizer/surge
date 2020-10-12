#pragma once

#include <string>
#include <vector>

/*
 * For a discussino of why this is in src/common rather than src/common/gui
 *
 * 1. It is consumed by the SkinModel.cpp
 * 2. So see the discussion in SkinModel.h
 *
 */

namespace Surge
{
   namespace Skin
   {
      struct Color {
         Color( std::string name, int r, int g, int b );
         Color( std::string name, int r, int g, int b, int a );

         static Color colorByName( const std::string &name );
         static std::vector<Color> getAllColors();
         std::string name;
         int r, g, b, a;
      };
   }
}
namespace Colors
{
   namespace AboutBox
   {
      extern const Surge::Skin::Color Text, Link;
   }

   namespace Dialog
   {
      extern const Surge::Skin::Color Background, Border;

      namespace Titlebar
      {
         extern const Surge::Skin::Color Background, Text;
      }
      namespace Button
      {
         extern const Surge::Skin::Color Background, Border, Text, BackgroundHover, BorderHover, TextHover;
      }
      namespace Checkbox
      {
         extern const Surge::Skin::Color Background, Border, Tick;
      }
      namespace Entry
      {
         extern const Surge::Skin::Color Background, Border, Focus, Text;
      }
      namespace Label
      {
         extern const Surge::Skin::Color Error, Text;
      }
   }

   namespace Effect
   {
      namespace Label
      {
         extern const Surge::Skin::Color Text, Separator;
      }
      namespace Preset
      {
         extern const Surge::Skin::Color Name;
      }
      namespace Menu
      {
         extern const Surge::Skin::Color Text;
      }
      namespace SelectorPanel {
         extern const Surge::Skin::Color LabelBorder;
      }
   }

   namespace InfoWindow
   {
      extern const Surge::Skin::Color Background, Border, Text;

      namespace Modulation
      {
         extern const Surge::Skin::Color Negative, Positive, ValueNegative, ValuePositive;
      }
   }
   
   namespace LFO
   {
      namespace Title
      {
         extern const Surge::Skin::Color Text;
      }
      namespace Type
      {
         extern const Surge::Skin::Color Text, SelectedText, SelectedBackground;
      }
      namespace StepSeq
      {
         extern const Surge::Skin::Color Background, ColumnShadow, DragLine, Envelope, TriggerClick, Wave;

         namespace Button
         {
            extern const Surge::Skin::Color Background, Border, Hover, Arrow;
         }
         namespace InfoWindow
         {
            extern const Surge::Skin::Color Background, Border, Text;
         }
         namespace Loop
         {
            extern const Surge::Skin::Color Marker, PrimaryStep, SecondaryStep, OutsidePrimaryStep, OutsideSecondaryStep;
         }
         namespace Step
         {
            extern const Surge::Skin::Color Fill, FillDeactivated, FillOutside;
         }
      }
      namespace Waveform
      {
         extern const Surge::Skin::Color Background, Bounds, Center, Envelope, Wave, DeactivatedWave;

         namespace Ruler
         {
            extern const Surge::Skin::Color Text, Ticks, ExtendedTicks, SmallTicks;
         }
      }
   }

   namespace Menu
   {
      extern const Surge::Skin::Color Name, NameHover, Value, ValueHover, ValueDeactivated, NameDeactivated;
      extern const Surge::Skin::Color FilterValue, FilterValueHover;
   }

   namespace ModSource
   {
      namespace Unused
      {
          extern const Surge::Skin::Color Background, Border, BorderHover, Text, TextHover;
      }
      namespace Used
      {
          extern const Surge::Skin::Color Background, Border, BorderHover, Text, TextHover, UsedModHover;
      }
      namespace Selected
      {
          extern const Surge::Skin::Color Background, Border, BorderHover, Text, TextHover, UsedModHover;

          namespace Used
          {
              extern const Surge::Skin::Color Background, Border, BorderHover, Text, TextHover, UsedModHover;
          }
      }
      namespace Armed
      {
          extern const Surge::Skin::Color Background, Border, BorderHover, Text, TextHover, UsedModHover;
      }
   }

   namespace MSEGEditor
   {
      extern const Surge::Skin::Color Background, Curve, Panel, Text;

      namespace Axis
      {
          extern const Surge::Skin::Color Line, Text;
      }
      namespace GradientFill
      {
          extern const Surge::Skin::Color StartColor, EndColor;
      }
      namespace Grid
      {
          extern const Surge::Skin::Color Primary, Secondary;
      }
      namespace Loop
      {
          extern const Surge::Skin::Color Line;
      }
      namespace NumberField
      {
         extern const Surge::Skin::Color Background, Text, Border;
      }
   }

   namespace NumberField
   {
      extern const Surge::Skin::Color DefaultText, DefaultHoverText;
   }

   namespace Scene {
      extern const Surge::Skin::Color PitchBendText,
                  PitchBendTextHover,
                  SplitPolyText,
                  SplitPolyTextHover,
                  KeytrackText,
                  KeytrackTextHover;
   }

   namespace Osc
   {
      namespace Display
      {
         extern const Surge::Skin::Color Bounds, Center, AnimatedWave, Wave;
      }
      namespace Filename
      {
         extern const Surge::Skin::Color Background, Text;
      }
   }

   namespace Overlay
   {
      extern const Surge::Skin::Color Background;
   }

   namespace PatchBrowser
   {
      extern const Surge::Skin::Color Background, Text;
   }

   namespace Slider
   {
      namespace Label
      {
         extern const Surge::Skin::Color Light, Dark;
      }
      namespace Modulation
      {
         extern const Surge::Skin::Color Positive, Negative;
      }
   }

   namespace StatusButton
   {
      extern const Surge::Skin::Color Background, Border, Text, SelectedBackground, SelectedText;
   }

   namespace VuMeter
   {
      extern const Surge::Skin::Color Level, HighLevel, Border, Background;
   }

}

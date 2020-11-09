#pragma once

#include <string>
#include <vector>

/*
 * For a discussion of why this is in src/common rather than src/common/gui
 *
 * 1. It is consumed by the SkinModel.cpp
 * 2. See the discussion in SkinModel.h
 *
 */

namespace Surge
{
   namespace Skin
   {
      struct Color {
         Color(std::string name, int r, int g, int b);
         Color(std::string name, int r, int g, int b, int a);

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
         extern const Surge::Skin::Color Text, TextHover;
      }
      namespace Grid
      {
         extern const Surge::Skin::Color Border;
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
      extern const Surge::Skin::Color Name, NameHover, NameDeactivated, Value, ValueHover, ValueDeactivated, FilterValue, FilterValueHover;
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
      namespace Macro
      {
          extern const Surge::Skin::Color Background, Fill;
      }
   }

   namespace MSEGEditor
   {
      extern const Surge::Skin::Color Background, Curve, DeformCurve, CurveHighlight, Panel, Text;

      namespace Axis
      {
          extern const Surge::Skin::Color Line, Text, SecondaryText;
      }
      namespace GradientFill
      {
          extern const Surge::Skin::Color StartColor, EndColor;
      }
      namespace Grid
      {
          extern const Surge::Skin::Color Primary, SecondaryHorizontal, SecondaryVertical;
      }
      namespace Loop
      {
          extern const Surge::Skin::Color Line, Marker, MarkerHover;
      }
      namespace NumberField
      {
         extern const Surge::Skin::Color Text, TextHover;
      }
   }

   namespace NumberField
   {
      extern const Surge::Skin::Color Text, TextHover;
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

   namespace Scene
   {
      namespace PitchBendRange
      {
         extern const Surge::Skin::Color Text, HoverText;
      }
      namespace SplitPoint
      {
         extern const Surge::Skin::Color Text, HoverText;
      }
      namespace KeytrackRoot
      {
         extern const Surge::Skin::Color Text, HoverText;
      }
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

   namespace VuMeter
   {
      extern const Surge::Skin::Color Level, HighLevel, Border, Background;
   }
}

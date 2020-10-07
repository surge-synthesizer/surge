#pragma once

#include "SkinSupport.h"

namespace Colors
{
   namespace AboutBox
   {
      extern const Surge::UI::SkinColor Text, Link;
   }

   namespace Dialog
   {
      extern const Surge::UI::SkinColor Background, Border;

      namespace Titlebar
      {
         extern const Surge::UI::SkinColor Background, Text;
      }
      namespace Button
      {
         extern const Surge::UI::SkinColor Background, Border, Text;
      }
      namespace Checkbox
      {
         extern const Surge::UI::SkinColor Background, Border, Tick;
      }
      namespace Entry
      {
         extern const Surge::UI::SkinColor Background, Border, Focus, Text;
      }
      namespace Label
      {
         extern const Surge::UI::SkinColor Error, Text;
      }
   }

   namespace Effect
   {
      namespace Label
      {
         extern const Surge::UI::SkinColor Text, Line;
      }
      namespace Preset
      {
         extern const Surge::UI::SkinColor Name;
      }
      namespace Menu
      {
         extern const Surge::UI::SkinColor Text;
      }
   }

   namespace InfoWindow
   {
      extern const Surge::UI::SkinColor Background, Border, Text;

      namespace Modulation
      {
         extern const Surge::UI::SkinColor Negative, Positive, ValueNegative, ValuePositive;
      }
   }
   
   namespace LFO
   {
      namespace Title
      {
         extern const Surge::UI::SkinColor Text;
      }
      namespace Type
      {
         extern const Surge::UI::SkinColor Text, SelectedText, SelectedBackground;
      }
      namespace StepSeq
      {
         extern const Surge::UI::SkinColor Background, ColumnShadow, DragLine, Envelope, TriggerClick, Wave;

         namespace Button
         {
            extern const Surge::UI::SkinColor Background, Border, Hover, Arrow;
         }
         namespace InfoWindow
         {
            extern const Surge::UI::SkinColor Background, Border, Text;
         }
         namespace Loop
         {
            extern const Surge::UI::SkinColor Marker, MinorStep, MajorStep, OutsideMinorStep, OutsideMajorStep;
         }
         namespace Step
         {
            extern const Surge::UI::SkinColor Fill, OutsideFill, FillDeactivated;
         }
      }
      namespace Waveform
      {
         extern const Surge::UI::SkinColor Background, Bounds, CenterLine, Envelope, Fill, MajorDivisions, Wave;

         namespace Ruler
         {
            extern const Surge::UI::SkinColor Text, Ticks, SmallTicks;
         }
      }
   }

   namespace Menu
   {
      extern const Surge::UI::SkinColor Name, NameHover, Value, ValueHover, ValueDeactivated, NameDeactivated;
      extern const Surge::UI::SkinColor FilterValue, FilterValueHover;
   }

   namespace ModSource
   {
      namespace Unused
      {
          extern const Surge::UI::SkinColor Background, Border, BorderHover, Text, TextHover;
      }
      namespace Used
      {
          extern const Surge::UI::SkinColor Background, Border, BorderHover, Text, TextHover, UsedModHover;
      }
      namespace Selected
      {
          extern const Surge::UI::SkinColor Background, Border, BorderHover, Text, TextHover, UsedModHover;

          namespace Used
          {
              extern const Surge::UI::SkinColor Background, Border, BorderHover, Text, TextHover, UsedModHover;
          }
      }
      namespace Armed
      {
          extern const Surge::UI::SkinColor Background, Border, BorderHover, Text, TextHover, UsedModHover;
      }
   }

   namespace MSEGEditor
   {
      extern const Surge::UI::SkinColor Background, Line, Panel;

      namespace Axis
      {
          extern const Surge::UI::SkinColor Line;
      }
      namespace GradientFill
      {
          extern const Surge::UI::SkinColor StartColor, EndColor;
      }
      namespace Grid
      {
          extern const Surge::UI::SkinColor Primary, Secondary;
      }
      namespace Loop
      {
          extern const Surge::UI::SkinColor Line;
      }
   }

   namespace NumberField
   {
      extern const Surge::UI::SkinColor Background, Text, Border, Env;
   }

   namespace Osc
   {
      namespace Display
      {
         extern const Surge::UI::SkinColor Bounds, CenterLine, AnimatedWave, Wave;
      }
      namespace Filename
      {
         extern const Surge::UI::SkinColor Background, Text;
      }
   }

   namespace Overlay
   {
      extern const Surge::UI::SkinColor Background, Border;

      namespace Titlebar
      {
         extern const Surge::UI::SkinColor Background, Text;
      }
   }

   namespace PatchBrowser
   {
      extern const Surge::UI::SkinColor Background, Text;
   }

   namespace Slider
   {
      namespace Label
      {
         extern const Surge::UI::SkinColor Light, Dark;
      }
      namespace Modulation
      {
         extern const Surge::UI::SkinColor Positive, Negative;
      }
   }

   namespace StatusButton
   {
      extern const Surge::UI::SkinColor Background, Border, Text, SelectedBackground, SelectedText;
   }

   namespace VuMeter
   {
      extern const Surge::UI::SkinColor Level, HighLevel, Border, Background;
   }

}

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
            extern const Surge::UI::SkinColor Fill, OutsideFill;
         }
      }
      namespace Waveform
      {
         extern const Surge::UI::SkinColor Background, Bounds, CenterLine, Envelope, Fill, MajorDivisions, Wave;

         namespace Ruler
         {
            extern const Surge::UI::SkinColor Text, Ticks;
         }
      }
   }

   namespace Menu
   {
      extern const Surge::UI::SkinColor Name, Value, ValueHover;
   }

   namespace ModSource
   {
      namespace Active
      {
          extern const Surge::UI::SkinColor Background, Border, BorderHover, Text, TextHover, UsedHover;
      }
      namespace Inactive
      {
          extern const Surge::UI::SkinColor Background, Border, BorderHover, Text, TextHover, UsedHover;
      }
      namespace Selected
      {
          extern const Surge::UI::SkinColor Background, Border, BorderHover, Text, TextHover, UsedHover;
      }
      namespace Used
      {
          extern const Surge::UI::SkinColor Background, Border, BorderHover, Text, TextHover;
      }
   }

   namespace MSEGEditor
   {
      extern const Surge::UI::SkinColor Line;

      namespace Axis
      {
          extern const Surge::UI::SkinColor Line;
      }
      namespace Grid
      {
          extern const Surge::UI::SkinColor Primary, Secondary;
      }
      namespace Loop
      {
          extern const Surge::UI::SkinColor Line;
      }
      namespace Segment
      {
          extern const Surge::UI::SkinColor Hover;
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

      namespace Container
      {
         extern const Surge::UI::SkinColor Background, TitleText;
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

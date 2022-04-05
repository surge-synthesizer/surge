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
struct Color
{
    Color(const std::string &name, int r, int g, int b);
    Color(const std::string &name, int r, int g, int b, int a);
    Color(const std::string &name, uint32_t argb);
    Color(const std::string &name, uint32_t rgb, char alpha);

    static Color colorByName(const std::string &name);
    static std::vector<Color> getAllColors();

    std::string name;
    uint8_t r, g, b, a;
};
} // namespace Skin
} // namespace Surge
namespace Colors
{
namespace AboutPage
{
extern const Surge::Skin::Color Text, ColumnText, Link, LinkHover;
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
extern const Surge::Skin::Color Background, Border, Text, BackgroundHover, BorderHover, TextHover,
    BackgroundPressed, BorderPressed, TextPressed;
}
namespace Checkbox
{
extern const Surge::Skin::Color Background, Border, Tick;
}
namespace Entry
{
extern const Surge::Skin::Color Background, Border, Caret, Focus, Text;
}
namespace Label
{
extern const Surge::Skin::Color Error, Text;
}
} // namespace Dialog

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

namespace Scene
{
extern const Surge::Skin::Color Background, Border, Text, BackgroundHover, BorderHover, TextHover;
}
namespace Unselected
{
extern const Surge::Skin::Color Background, Border, Text, BackgroundHover, BorderHover, TextHover;
}

namespace Selected
{
extern const Surge::Skin::Color Background, Border, Text, BackgroundHover, BorderHover, TextHover;
}

namespace Bypassed
{
extern const Surge::Skin::Color Background, Border, Text, BackgroundHover, BorderHover, TextHover;
}

namespace BypassedSelected
{
extern const Surge::Skin::Color Background, Border, Text, BackgroundHover, BorderHover, TextHover;
}
} // namespace Grid
} // namespace Effect

namespace FormulaEditor
{
extern const Surge::Skin::Color Background, Highlight, Text, LineNumBackground, LineNumText;

namespace Debugger
{
extern const Surge::Skin::Color Row, LightRow, Text, InternalText;
}

namespace Lua
{
extern const Surge::Skin::Color Bracket, Comment, Error, Identifier, Interpunction, Number, Keyword,
    String;
} // namespace Lua
} // namespace FormulaEditor

namespace InfoWindow
{
extern const Surge::Skin::Color Background, Border, Text;

namespace Modulation
{
extern const Surge::Skin::Color Negative, Positive, ValueNegative, ValuePositive;
}
} // namespace InfoWindow

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
extern const Surge::Skin::Color Background, Border, Hover, Arrow, ArrowHover;
}
namespace InfoWindow
{
extern const Surge::Skin::Color Background, Border, Text;
}
namespace Loop
{
extern const Surge::Skin::Color Marker, PrimaryStep, SecondaryStep, OutsidePrimaryStep,
    OutsideSecondaryStep;
}
namespace Step
{
extern const Surge::Skin::Color Fill, FillDeactivated, FillOutside;
}
} // namespace StepSeq
namespace Waveform
{
extern const Surge::Skin::Color Background, Bounds, Center, Envelope, Wave, GhostedWave,
    DeactivatedWave, Dots;

namespace Ruler
{
extern const Surge::Skin::Color Text, Ticks, ExtendedTicks, SmallTicks;
}
} // namespace Waveform
} // namespace LFO

namespace Menu
{
extern const Surge::Skin::Color Name, NameHover, NameDeactivated, Value, ValueHover,
    ValueDeactivated, FilterValue, FilterValueHover;
}

namespace PopupMenu
{
extern const Surge::Skin::Color Background, HiglightedBackground, Text, HeaderText, HighlightedText;
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
} // namespace Selected
namespace Armed
{
extern const Surge::Skin::Color Background, Border, BorderHover, Text, TextHover, UsedModHover;
}
namespace Macro
{
extern const Surge::Skin::Color Background, Border, Fill, CurrentValue;
}
} // namespace ModSource

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
extern const Surge::Skin::Color Marker, RegionAxis, RegionFill, RegionBorder;
}
namespace NumberField
{
extern const Surge::Skin::Color Text, TextHover;
}
} // namespace MSEGEditor

namespace NumberField
{
extern const Surge::Skin::Color Text, TextHover;
}

namespace Osc
{
namespace Display
{
extern const Surge::Skin::Color Bounds, Center, AnimatedWave, WaveCurrent3D, WaveStart3D, WaveEnd3D,
    WaveFillStart3D, WaveFillEnd3D, Wave, Dots;
}
namespace Filename
{
extern const Surge::Skin::Color Background, BackgroundHover, Frame, FrameHover, Text, TextHover;
}
namespace Type
{
extern const Surge::Skin::Color Text, TextHover;
}
} // namespace Osc

namespace Waveshaper
{
extern const Surge::Skin::Color Text, TextHover;
namespace Display
{
extern const Surge::Skin::Color Dots, Wave, WaveHover;
}
namespace Preview
{
extern const Surge::Skin::Color Border, Background, Text;
}
} // namespace Waveshaper

namespace Overlay
{
extern const Surge::Skin::Color Background;
}

namespace PatchBrowser
{
extern const Surge::Skin::Color Text, TextHover;

namespace CommentTooltip
{
extern const Surge::Skin::Color Border, Background, Text;
}
namespace TypeAheadList
{
extern const Surge::Skin::Color Border, Background, Text, HighlightBackground, HighlightText,
    SubText, HighlightSubText, Separator;
}
} // namespace PatchBrowser

namespace Scene
{
namespace PitchBendRange
{
extern const Surge::Skin::Color Text, TextHover;
}
namespace SplitPoint
{
extern const Surge::Skin::Color Text, TextHover;
}
namespace KeytrackRoot
{
extern const Surge::Skin::Color Text, TextHover;
}
} // namespace Scene

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
} // namespace Slider

namespace VuMeter
{
extern const Surge::Skin::Color Border, Background, UnavailableText;
}

namespace TuningOverlay
{
namespace FrequencyKeyboard
{
extern const Surge::Skin::Color WhiteKey, BlackKey, Separator, Text, PressedKey, PressedKeyText;
}
namespace SCLKBM
{
extern const Surge::Skin::Color Background;
namespace Editor
{
extern const Surge::Skin::Color Border, Background, Text, Comment, Cents, Ratio, Played;
} // namespace Editor
} // namespace SCLKBM
namespace RadialGraph
{
extern const Surge::Skin::Color Background;
extern const Surge::Skin::Color KnobFill, KnobFillHover, KnobBorder, KnobThumb, KnobThumbPlayed;
extern const Surge::Skin::Color ToneLabel, ToneLabelPlayed, ToneLabelBorder, ToneLabelBackground,
    ToneLabelText, ToneLabelBackgroundPlayed, ToneLabelBorderPlayed, ToneLabelTextPlayed;
} // namespace RadialGraph
namespace Interval
{
extern const Surge::Skin::Color Background;
extern const Surge::Skin::Color NoteLabelBackground, NoteLabelBackgroundPlayed, NoteLabelForeground,
    NoteLabelForegroundHover, NoteLabelForegroundPlayed, NoteLabelForegroundHoverPlayed;
extern const Surge::Skin::Color IntervalText, IntervalTextHover, IntervalSkipped;

extern const Surge::Skin::Color HeatmapZero, HeatmapNegFar, HeatmapNegNear, HeatmapPosFar,
    HeatmapPosNear;
} // namespace Interval
} // namespace TuningOverlay

namespace VirtualKeyboard
{
extern const Surge::Skin::Color Text, Shadow;

namespace Wheel
{
extern const Surge::Skin::Color Background, Border, Value;
}

namespace Key
{
extern const Surge::Skin::Color Black, White, Separator, MouseOver, Pressed;
}
namespace OctaveJog
{
extern const Surge::Skin::Color Background, Arrow;
} // namespace OctaveJog
} // namespace VirtualKeyboard

namespace ModulationListOverlay
{
extern const Surge::Skin::Color Border, Text, DimText, Arrows;
}

namespace JuceWidgets
{
namespace TabbedBar
{
extern const Surge::Skin::Color ActiveTabBackground, InactiveTabBackground, Border, Text, TextHover;
}
namespace TextMultiSwitch
{
extern const Surge::Skin::Color Background, Border, Separator, DeactivatedText, Text, OnText,
    OnFill, TextHover, HoverOnText, HoverFill, HoverOnFill, HoverOnBorder, UnpressedHighlight;
}
} // namespace JuceWidgets
} // namespace Colors

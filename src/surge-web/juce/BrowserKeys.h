/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2013 - Raw Material Software Ltd.

   Permission is granted to use this software under the terms of either:
   a) the GPL v2 (or any later version)
   b) the Affero GPL v3

   Details of these licenses can be found at: www.gnu.org/licenses

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.juce.com for more information.

  ==============================================================================
*/

// Based on Dreamtonics/juce_emscripten; see web/README.md.
// Function keys use a distinct range so F1-F12 cannot alias lowercase p-z.
namespace juce
{
const int extendedKeyModifier = 0x10000;

const int KeyPress::spaceKey = 32;
const int KeyPress::returnKey = 13;
const int KeyPress::escapeKey = 27;
const int KeyPress::backspaceKey = 8;
const int KeyPress::leftKey = 37;
const int KeyPress::rightKey = 39;
const int KeyPress::upKey = 38;
const int KeyPress::downKey = 40;
const int KeyPress::pageUpKey = 33;
const int KeyPress::pageDownKey = 34;
const int KeyPress::endKey = 35;
const int KeyPress::homeKey = 36;
const int KeyPress::deleteKey = 46;
const int KeyPress::insertKey = 45;
const int KeyPress::tabKey = 9;
const int KeyPress::F1Key = extendedKeyModifier + 256;
const int KeyPress::F2Key = extendedKeyModifier + 257;
const int KeyPress::F3Key = extendedKeyModifier + 258;
const int KeyPress::F4Key = extendedKeyModifier + 259;
const int KeyPress::F5Key = extendedKeyModifier + 260;
const int KeyPress::F6Key = extendedKeyModifier + 261;
const int KeyPress::F7Key = extendedKeyModifier + 262;
const int KeyPress::F8Key = extendedKeyModifier + 263;
const int KeyPress::F9Key = extendedKeyModifier + 264;
const int KeyPress::F10Key = extendedKeyModifier + 265;
const int KeyPress::F11Key = extendedKeyModifier + 266;
const int KeyPress::F12Key = extendedKeyModifier + 267;
const int KeyPress::F13Key = extendedKeyModifier + 268;
const int KeyPress::F14Key = extendedKeyModifier + 269;
const int KeyPress::F15Key = extendedKeyModifier + 270;
const int KeyPress::F16Key = extendedKeyModifier + 271;
const int KeyPress::F17Key = extendedKeyModifier + 272;
const int KeyPress::F18Key = extendedKeyModifier + 273;
const int KeyPress::F19Key = extendedKeyModifier + 274;
const int KeyPress::F20Key = extendedKeyModifier + 275;
const int KeyPress::F21Key = extendedKeyModifier + 276;
const int KeyPress::F22Key = extendedKeyModifier + 277;
const int KeyPress::F23Key = extendedKeyModifier + 278;
const int KeyPress::F24Key = extendedKeyModifier + 279;
const int KeyPress::F25Key = extendedKeyModifier + 280;
const int KeyPress::F26Key = extendedKeyModifier + 281;
const int KeyPress::F27Key = extendedKeyModifier + 282;
const int KeyPress::F28Key = extendedKeyModifier + 283;
const int KeyPress::F29Key = extendedKeyModifier + 284;
const int KeyPress::F30Key = extendedKeyModifier + 285;
const int KeyPress::F31Key = extendedKeyModifier + 286;
const int KeyPress::F32Key = extendedKeyModifier + 287;
const int KeyPress::F33Key = extendedKeyModifier + 288;
const int KeyPress::F34Key = extendedKeyModifier + 289;
const int KeyPress::F35Key = extendedKeyModifier + 290;
const int KeyPress::numberPad0 = extendedKeyModifier + 27;
const int KeyPress::numberPad1 = extendedKeyModifier + 28;
const int KeyPress::numberPad2 = extendedKeyModifier + 29;
const int KeyPress::numberPad3 = extendedKeyModifier + 30;
const int KeyPress::numberPad4 = extendedKeyModifier + 31;
const int KeyPress::numberPad5 = extendedKeyModifier + 32;
const int KeyPress::numberPad6 = extendedKeyModifier + 33;
const int KeyPress::numberPad7 = extendedKeyModifier + 34;
const int KeyPress::numberPad8 = extendedKeyModifier + 35;
const int KeyPress::numberPad9 = extendedKeyModifier + 36;
const int KeyPress::numberPadAdd = extendedKeyModifier + 37;
const int KeyPress::numberPadSubtract = extendedKeyModifier + 38;
const int KeyPress::numberPadMultiply = extendedKeyModifier + 39;
const int KeyPress::numberPadDivide = extendedKeyModifier + 40;
const int KeyPress::numberPadSeparator = extendedKeyModifier + 41;
const int KeyPress::numberPadDecimalPoint = extendedKeyModifier + 42;
const int KeyPress::numberPadEquals = extendedKeyModifier + 43;
const int KeyPress::numberPadDelete = extendedKeyModifier + 44;
const int KeyPress::playKey = extendedKeyModifier + 45;
const int KeyPress::stopKey = extendedKeyModifier + 46;
const int KeyPress::fastForwardKey = extendedKeyModifier + 47;
const int KeyPress::rewindKey = extendedKeyModifier + 48;

} // namespace juce

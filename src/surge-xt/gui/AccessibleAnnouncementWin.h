/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the VCS
 * history and in the readme file in the root of this repository.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#pragma once

#include <memory>
#include <string>

namespace Surge
{
namespace GUI
{

/*
 * Speaks accessibility announcements through the running screen reader
 * (NVDA, Narrator, JAWS, ...) rather than through SAPI, which is what
 * juce::AccessibilityHandler::postAnnouncement does on Windows.
 *
 * It works by raising UIA notification events (UiaRaiseNotificationEvent,
 * Windows 10 1709+) on a hidden, unfocusable 1x1 child window of the editor,
 * which carries a minimal UIA provider marked as neither a control nor a
 * content element so it never appears in keyboard or object navigation.
 * The same approach is used by OSARA, the REAPER accessibility extension.
 */
struct UiaAnnouncer
{
    // parentWindowHandle is the HWND hosting the editor, i.e.
    // frame->getPeer()->getNativeHandle()
    explicit UiaAnnouncer(void *parentWindowHandle);
    ~UiaAnnouncer();

    // Returns true if the announcement was raised as a UIA notification.
    // Returns false when the API is unavailable (pre-1709 Windows) or no UIA
    // client is listening, in which case the caller should fall back to
    // juce::AccessibilityHandler::postAnnouncement.
    bool announce(const std::string &text, bool interrupt);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace GUI
} // namespace Surge

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

#include "MSEGEditorAccessibleKeyboard.h"

#include "SurgeStorage.h"
#include "MSEGModulationHelper.h"

#include "fmt/core.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Surge
{
namespace Overlays
{

namespace
{
// SegmentProps values from MSEGCanvas, which is local to MSEGEditor.cpp
constexpr int kTypeinValue = 1;
constexpr int kTypeinCPValue = 3;
constexpr int kTypein2DChooser = -1;

// matches MSEGCanvas::TimeEdit
constexpr int kTimeEditShift = 1;

// the CP hotzone existence condition from MSEGCanvas::recalcHotZones
constexpr float kMinCPDuration = 0.01f;

std::string segmentTypeName(MSEGStorage::segment::Type t)
{
    switch (t)
    {
    case MSEGStorage::segment::LINEAR:
        return "Linear";
    case MSEGStorage::segment::QUAD_BEZIER:
        return "Bezier";
    case MSEGStorage::segment::SCURVE:
        return "S-Curve";
    case MSEGStorage::segment::SINE:
        return "Sine";
    case MSEGStorage::segment::STAIRS:
        return "Stairs";
    case MSEGStorage::segment::BROWNIAN:
        return "Brownian Bridge";
    case MSEGStorage::segment::SQUARE:
        return "Square";
    case MSEGStorage::segment::TRIANGLE:
        return "Triangle";
    case MSEGStorage::segment::HOLD:
        return "Hold";
    case MSEGStorage::segment::SAWTOOTH:
        return "Sawtooth";
    case MSEGStorage::segment::BUMP:
        return "Bump";
    case MSEGStorage::segment::SMOOTH_STAIRS:
        return "Smooth Stairs";
    default:
        return "Unknown";
    }
}

char lowerChar(const juce::KeyPress &key)
{
    auto tc = key.getTextCharacter();
    if (tc >= 'A' && tc <= 'Z')
        return (char)(tc - 'A' + 'a');
    if (tc >= 'a' && tc <= 'z')
        return (char)tc;

    auto kc = key.getKeyCode();
    if (kc >= 'A' && kc <= 'Z')
        return (char)(kc - 'A' + 'a');
    if (kc >= 'a' && kc <= 'z')
        return (char)kc;

    return 0;
}

// letter shortcuts require ALT (Option on mac) so bare letters never trigger actions
char altLetter(const juce::KeyPress &key)
{
    if (!key.getModifiers().isAltDown())
        return 0;
    return lowerChar(key);
}

float quantizeValue(float v, float snapResolution)
{
    if (snapResolution > 0)
    {
        float q = v + 1;
        float pos = std::round(q / snapResolution) * snapResolution;
        v = pos - 1.0f;
    }
    return std::clamp(v, -1.f, 1.f);
}
} // namespace

int MSEGAccessibleKeyboardHandler::numNodes() const { return ms->n_activeSegments + 1; }

float MSEGAccessibleKeyboardHandler::nodeTime(int i) const
{
    if (i >= ms->n_activeSegments)
        return ms->totalDuration;
    return ms->segmentStart[std::max(i, 0)];
}

float MSEGAccessibleKeyboardHandler::nodeValue(int i) const
{
    if (i >= ms->n_activeSegments)
        return ms->segments[ms->n_activeSegments - 1].nv1;
    return ms->segments[std::max(i, 0)].v0;
}

bool MSEGAccessibleKeyboardHandler::controlPointUsable(int seg) const
{
    return seg >= 0 && seg < ms->n_activeSegments && ms->segments[seg].duration > kMinCPDuration &&
           ms->segments[seg].type != MSEGStorage::segment::HOLD;
}

bool MSEGAccessibleKeyboardHandler::controlPointIs2D(int seg) const
{
    auto t = ms->segments[seg].type;
    return t == MSEGStorage::segment::QUAD_BEZIER || t == MSEGStorage::segment::BROWNIAN;
}

std::pair<float, float> MSEGAccessibleKeyboardHandler::cursorPosition() const
{
    return {nodeTime(index), nodeValue(index)};
}

int MSEGAccessibleKeyboardHandler::segmentForCursor() const
{
    return std::min(index, ms->n_activeSegments - 1);
}

int MSEGAccessibleKeyboardHandler::selectableNodeCount() const { return ms->n_activeSegments; }

float MSEGAccessibleKeyboardHandler::unipolarFactor() const
{
    return 1.f + (lfodata && lfodata->unipolar.val.b ? 1.f : 0.f);
}

float MSEGAccessibleKeyboardHandler::xStep(const juce::ModifierKeys &mods) const
{
    float b = ms->hSnap > 0 ? ms->hSnap : ms->hSnapDefault;
    if (b <= 0)
        b = 0.05f;
    if (mods.isShiftDown())
        b *= 0.25f;
    if (mods.isCommandDown())
        b *= 4.f;
    return b;
}

float MSEGAccessibleKeyboardHandler::yStep(const juce::ModifierKeys &mods) const
{
    float b = ms->vSnap > 0 ? ms->vSnap : ms->vSnapDefault;
    if (b <= 0)
        b = 0.025f;
    b *= unipolarFactor();
    if (mods.isShiftDown())
        b *= 0.25f;
    if (mods.isCommandDown())
        b *= 4.f;
    return b;
}

float MSEGAccessibleKeyboardHandler::xSnapFor(const juce::ModifierKeys &mods) const
{
    return mods.isShiftDown() ? 0.f : ms->hSnap;
}

float MSEGAccessibleKeyboardHandler::ySnapFor(const juce::ModifierKeys &mods) const
{
    return mods.isShiftDown() ? 0.f : ms->vSnap * unipolarFactor();
}

float MSEGAccessibleKeyboardHandler::curveValueAt(float t) const
{
    Surge::MSEG::EvaluatorState es;
    auto iup = (int)t;
    auto fup = t - iup;
    return Surge::MSEG::valueAt(iup, fup, 0.f, ms, &es, true);
}

std::string MSEGAccessibleKeyboardHandler::formatValue(float v) const
{
    return fmt::format("{:.{}f}", v, Surge::Storage::getValueDisplayPrecision(storage));
}

std::string MSEGAccessibleKeyboardHandler::nodeAnnouncement(int i) const
{
    auto N = ms->n_activeSegments;
    auto s = fmt::format("Node {} of {}, time {}, value {}", i + 1, N + 1, formatValue(nodeTime(i)),
                         formatValue(nodeValue(i)));

    if (i == N && ms->endpointMode == MSEGStorage::EndpointMode::LOCKED)
        s += ", linked to start node";
    if (ms->loop_start >= 0 && i == ms->loop_start)
        s += ", loop start";
    if (ms->loop_end >= 0 && i == ms->loop_end + 1)
        s += ", loop end";

    if (i < N)
    {
        auto &seg = ms->segments[i];
        s += ", segment " + segmentTypeName(seg.type);
        if (seg.retriggerFEG)
            s += ", retriggers filter EG";
        if (seg.retriggerAEG)
            s += ", retriggers amp EG";
        if (!seg.useDeform)
            s += ", deform off";
        if (seg.invertDeform)
            s += ", deform inverted";
    }

    return s;
}

std::string MSEGAccessibleKeyboardHandler::cpAnnouncement(int seg) const
{
    auto &s = ms->segments[seg];
    auto base = fmt::format("Control point, segment {}, {}", seg + 1, segmentTypeName(s.type));

    if (controlPointIs2D(seg))
        return base + fmt::format(", X {}, Y {}", formatValue(s.cpduration), formatValue(s.cpv));

    return base + ", value " + formatValue(s.cpv);
}

void MSEGAccessibleKeyboardHandler::announceCursor()
{
    if (!cb.announce)
        return;
    cb.announce(nodeAnnouncement(index));
}

void MSEGAccessibleKeyboardHandler::announceFocus()
{
    if (!cb.announce)
        return;
    auto s = fmt::format("MSEG canvas, {} segments, {} mode. ", ms->n_activeSegments,
                         ms->editMode == MSEGStorage::EditMode::LFO ? "LFO" : "envelope");
    s += nodeAnnouncement(index);
    cb.announce(s);
}

void MSEGAccessibleKeyboardHandler::announceEditorState()
{
    if (!cb.announce)
        return;

    std::string modeName = mode == Mode::SELECTION ? "Selection" : "Cursor";

    auto nSel = cb.getSelection ? (int)cb.getSelection().size() : 0;
    auto s = fmt::format("MSEG canvas, {} segments, {} mode, {} mode active, {} nodes selected. ",
                         ms->n_activeSegments,
                         ms->editMode == MSEGStorage::EditMode::LFO ? "LFO" : "envelope", modeName,
                         nSel);
    s += nodeAnnouncement(index);

    auto seg = segmentForCursor();
    if (controlPointUsable(seg))
        s += ". " + cpAnnouncement(seg);

    cb.announce(s);
}

void MSEGAccessibleKeyboardHandler::placeCursorOnNode(int node)
{
    index = std::clamp(node, 0, numNodes() - 1);
    rememberedTime = nodeTime(index);
    if (cb.repaint)
        cb.repaint();
}

// navigating in cursor mode abandons any selection
void MSEGAccessibleKeyboardHandler::moveCursor(int direction)
{
    auto pre = std::string(hasSelection() ? "Selection cleared. " : "");
    if (!pre.empty())
        clearSelection();

    auto n = numNodes();
    placeCursorOnNode(((index + direction) % n + n) % n);
    if (cb.announce)
        cb.announce(pre + nodeAnnouncement(index));
}

void MSEGAccessibleKeyboardHandler::nudgeNodeY(int node, float dy, float snapRes,
                                               bool announceResult)
{
    auto N = ms->n_activeSegments;
    bool linked = node >= N && ms->endpointMode == MSEGStorage::EndpointMode::LOCKED;
    float before = nodeValue(node);

    if (node >= N && !linked)
    {
        auto &seg = ms->segments[N - 1];
        auto v = quantizeValue(std::clamp(seg.nv1 + dy, -1.f, 1.f), snapRes);
        seg.nv1 = v;
        seg.dragv1 = v;
    }
    else
    {
        auto idx = linked ? 0 : node;
        auto &seg = ms->segments[idx];
        auto v = quantizeValue(std::clamp(seg.v0 + dy, -1.f, 1.f), snapRes);
        seg.v0 = v;
        seg.dragv0 = v;
    }

    inFlightEdit = true;
    if (cb.modelChanged)
        cb.modelChanged(std::min(node, N - 1));
    inFlightEdit = false;

    if (announceResult && cb.announce)
    {
        auto after = nodeValue(node);
        if (std::fabs(after - before) > 1e-7)
            cb.announce("Value " + formatValue(after) + (linked ? ", linked to start node" : ""));
        else
            cb.announce("At limit");
    }
}

void MSEGAccessibleKeyboardHandler::nudgeNodeX(int node, float dx, float snap, bool announceResult)
{
    auto N = ms->n_activeSegments;

    if (node <= 0)
    {
        if (cb.announce)
            cb.announce("Start node cannot move");
        return;
    }

    float before = nodeTime(node);

    if (node >= N)
    {
        if (ms->editMode == MSEGStorage::EditMode::LFO)
        {
            if (cb.announce)
                cb.announce("Final node time is fixed in LFO mode");
            return;
        }
        Surge::MSEG::adjustDurationShiftingSubsequent(ms, N - 1, dx, snap, max_msegs);
    }
    else
    {
        // per the accessibility spec, Draw movement mode nudges like Single
        if (cb.getTimeEditMode && cb.getTimeEditMode() == kTimeEditShift)
            Surge::MSEG::adjustDurationShiftingSubsequent(ms, node - 1, dx, snap, max_msegs);
        else
            Surge::MSEG::adjustDurationConstantTotalDuration(ms, node - 1, dx, snap);
    }

    inFlightEdit = true;
    if (cb.modelChanged)
        cb.modelChanged(std::min(node, N - 1));
    inFlightEdit = false;

    rememberedTime = nodeTime(node);

    if (announceResult && cb.announce)
    {
        auto after = nodeTime(node);
        if (std::fabs(after - before) > 1e-7)
            cb.announce("Time " + formatValue(after));
        else
            cb.announce("Constrained, did not move");
    }
}

void MSEGAccessibleKeyboardHandler::nudgeControlPoint(int seg, float dx, float dy)
{
    auto &s = ms->segments[seg];

    if (dx != 0)
    {
        if (!controlPointIs2D(seg))
        {
            if (cb.announce)
                cb.announce("Control point is vertical only");
            return;
        }

        float before = s.cpduration;
        s.cpduration += dx / std::max(s.duration, 0.001f);
        Surge::MSEG::constrainControlPointAt(ms, seg);

        inFlightEdit = true;
        if (cb.modelChanged)
            cb.modelChanged(seg);
        inFlightEdit = false;

        if (cb.announce)
        {
            if (std::fabs(s.cpduration - before) > 1e-7)
                cb.announce("X " + formatValue(s.cpduration));
            else
                cb.announce("At limit");
        }
    }

    if (dy != 0)
    {
        float before = s.cpv;
        s.cpv = std::clamp(s.cpv + dy, -1.f, 1.f);
        Surge::MSEG::constrainControlPointAt(ms, seg);

        inFlightEdit = true;
        if (cb.modelChanged)
            cb.modelChanged(seg);
        inFlightEdit = false;

        if (cb.announce)
        {
            if (std::fabs(s.cpv - before) > 1e-7)
                cb.announce((controlPointIs2D(seg) ? "Y " : "Value ") + formatValue(s.cpv));
            else
                cb.announce("At limit");
        }
    }
}

void MSEGAccessibleKeyboardHandler::addNode(bool append)
{
    auto N = ms->n_activeSegments;

    if (N >= max_msegs)
    {
        if (cb.announce)
            cb.announce(fmt::format("Maximum {} segments reached", max_msegs));
        return;
    }

    if (append)
    {
        if (ms->editMode == MSEGStorage::EditMode::LFO)
        {
            if (cb.announce)
                cb.announce("Cannot append in LFO mode");
            return;
        }

        float step = ms->hSnap > 0 ? ms->hSnap : ms->hSnapDefault;
        if (step <= 0)
            step = 0.125f;

        if (cb.prepareForUndo)
            cb.prepareForUndo();
        Surge::MSEG::extendTo(ms, ms->totalDuration + step, ms->segments[N - 1].nv1);
        if (cb.pushToUndo)
            cb.pushToUndo();

        inFlightEdit = true;
        if (cb.modelChanged)
            cb.modelChanged(ms->n_activeSegments - 1);
        inFlightEdit = false;

        placeCursorOnNode(numNodes() - 1);
        if (cb.announce)
            cb.announce("Added " + nodeAnnouncement(index));
    }
    else
    {
        auto seg = segmentForCursor();
        auto &s = ms->segments[seg];

        if (s.duration < 0.005f)
        {
            if (cb.announce)
                cb.announce("Segment too short to split");
            return;
        }

        auto t = ms->segmentStart[seg] + s.duration * 0.5f;

        if (cb.prepareForUndo)
            cb.prepareForUndo();
        Surge::MSEG::splitSegment(ms, t, curveValueAt(t));
        if (cb.pushToUndo)
            cb.pushToUndo();

        inFlightEdit = true;
        if (cb.modelChanged)
            cb.modelChanged(seg);
        inFlightEdit = false;

        placeCursorOnNode(seg + 1);
        if (cb.announce)
            cb.announce("Added " + nodeAnnouncement(index));
    }
}

void MSEGAccessibleKeyboardHandler::deleteNode(bool removeSegment)
{
    auto N = ms->n_activeSegments;

    if (index <= 0)
    {
        if (cb.announce)
            cb.announce("Start node cannot be deleted");
        return;
    }
    if (index >= N)
    {
        if (cb.announce)
            cb.announce("Final node cannot be deleted");
        return;
    }
    if (N <= 1)
    {
        if (cb.announce)
            cb.announce("Cannot delete the last remaining segment");
        return;
    }

    auto t = nodeTime(index);

    if (cb.prepareForUndo)
        cb.prepareForUndo();
    if (removeSegment)
        Surge::MSEG::deleteSegment(ms, t);
    else
        Surge::MSEG::unsplitSegment(ms, t);
    if (cb.pushToUndo)
        cb.pushToUndo();

    inFlightEdit = true;
    if (cb.modelChanged)
        cb.modelChanged(-1);
    inFlightEdit = false;

    placeCursorOnNode(std::min(index, numNodes() - 1));
    if (cb.announce)
        cb.announce("Deleted. " + nodeAnnouncement(index));
}

void MSEGAccessibleKeyboardHandler::resetCP()
{
    auto seg = segmentForCursor();

    if (!controlPointUsable(seg))
    {
        if (cb.announce)
            cb.announce("No control point on this segment");
        return;
    }

    if (cb.prepareForUndo)
        cb.prepareForUndo();
    Surge::MSEG::resetControlPoint(ms, seg);
    if (cb.pushToUndo)
        cb.pushToUndo();

    inFlightEdit = true;
    if (cb.modelChanged)
        cb.modelChanged(seg);
    inFlightEdit = false;

    if (cb.announce)
        cb.announce("Control point reset. " + cpAnnouncement(seg));
}

bool MSEGAccessibleKeyboardHandler::hasSelection() const
{
    return cb.getSelection && !cb.getSelection().empty();
}

void MSEGAccessibleKeyboardHandler::enterSelection()
{
    mode = Mode::SELECTION;

    // selection deals only in nodes 0 .. N-1, matching the mouse lasso
    if (index >= selectableNodeCount())
        placeCursorOnNode(selectableNodeCount() - 1);

    selectionAnchor = index;
    if (cb.setSelection)
        cb.setSelection({index});
    if (cb.announce)
        cb.announce("Selection mode on, 1 node selected");
}

void MSEGAccessibleKeyboardHandler::exitSelection()
{
    mode = Mode::CURSOR;
    if (cb.announce)
    {
        auto nSel = cb.getSelection ? (int)cb.getSelection().size() : 0;
        cb.announce(fmt::format("Selection mode off, {} nodes selected", nSel));
    }
}

void MSEGAccessibleKeyboardHandler::applyAnchorRange()
{
    if (!cb.setSelection)
        return;

    std::vector<int> sel;
    for (int i = std::min(selectionAnchor, index); i <= std::max(selectionAnchor, index); ++i)
        sel.push_back(i);
    cb.setSelection(sel);
}

void MSEGAccessibleKeyboardHandler::clearSelection()
{
    if (cb.setSelection)
    {
        cb.setSelection({});
        if (cb.repaint)
            cb.repaint();
    }
}

void MSEGAccessibleKeyboardHandler::selectAll()
{
    auto N = selectableNodeCount();
    std::vector<int> all;
    for (int i = 0; i < N; ++i)
        all.push_back(i);
    if (cb.setSelection)
        cb.setSelection(all);
    if (cb.announce)
        cb.announce(fmt::format("All nodes selected, {} nodes", N));
}

bool MSEGAccessibleKeyboardHandler::groupNudgeY(float dy, float snapRes)
{
    auto sel = cb.getSelection ? cb.getSelection() : std::vector<int>();
    auto N = ms->n_activeSegments;
    bool changed = false;

    for (auto i : sel)
    {
        if (i < 0 || i >= N)
            continue;
        auto &seg = ms->segments[i];
        auto before = seg.v0;
        auto v = quantizeValue(std::clamp(seg.v0 + dy, -1.f, 1.f), snapRes);
        seg.v0 = v;
        seg.dragv0 = v;
        changed = changed || std::fabs(v - before) > 1e-7;
    }

    inFlightEdit = true;
    if (cb.modelChanged)
        cb.modelChanged(-1);
    inFlightEdit = false;

    if (cb.announce)
    {
        if (changed)
            cb.announce(fmt::format("{} nodes moved, value {}", sel.size(),
                                    formatValue(nodeValue(std::min(index, N - 1)))));
        else
            cb.announce("At limit");
    }

    return changed;
}

bool MSEGAccessibleKeyboardHandler::groupNudgeX(float dx, float snap)
{
    auto sel = cb.getSelection ? cb.getSelection() : std::vector<int>();
    auto N = ms->n_activeSegments;

    std::sort(sel.begin(), sel.end());
    if (dx > 0)
        std::reverse(sel.begin(), sel.end());

    std::vector<float> before;
    for (auto i : sel)
        before.push_back(nodeTime(i));

    auto tem = cb.getTimeEditMode ? cb.getTimeEditMode() : 0;

    for (auto i : sel)
    {
        if (i <= 0 || i >= N)
            continue;

        auto prior = i - 1;

        // mirrors the lasso-aware timeConstraint logic in MSEGCanvas
        if (tem == kTimeEditShift)
        {
            bool isLast = true;
            for (auto si : sel)
                isLast = isLast && si <= prior + 1;

            if (isLast)
                Surge::MSEG::adjustDurationShiftingSubsequent(ms, prior, dx, snap, max_msegs);
            else
                Surge::MSEG::adjustDurationConstantTotalDuration(ms, prior, dx, snap);
        }
        else
        {
            Surge::MSEG::adjustDurationConstantTotalDuration(ms, prior, dx, snap);
        }

        Surge::MSEG::rebuildCache(ms);
    }

    inFlightEdit = true;
    if (cb.modelChanged)
        cb.modelChanged(-1);
    inFlightEdit = false;

    bool changed = false;
    for (size_t k = 0; k < sel.size(); ++k)
        changed = changed || std::fabs(nodeTime(sel[k]) - before[k]) > 1e-7;

    if (cb.announce)
    {
        if (changed)
            cb.announce(fmt::format("{} nodes moved", sel.size()));
        else
            cb.announce("Constrained, did not move");
    }

    return changed;
}

bool MSEGAccessibleKeyboardHandler::groupNudgeCP(float dx, float dy)
{
    auto sel = cb.getSelection ? cb.getSelection() : std::vector<int>();
    auto N = ms->n_activeSegments;
    int usable = 0, moved = 0;

    for (auto i : sel)
    {
        if (i < 0 || i >= N || !controlPointUsable(i))
            continue;
        if (dx != 0 && !controlPointIs2D(i))
            continue;

        usable++;
        auto &s = ms->segments[i];
        auto beforeX = s.cpduration;
        auto beforeY = s.cpv;

        if (dx != 0)
            s.cpduration += dx / std::max(s.duration, 0.001f);
        if (dy != 0)
            s.cpv = std::clamp(s.cpv + dy, -1.f, 1.f);
        Surge::MSEG::constrainControlPointAt(ms, i);

        if (std::fabs(s.cpduration - beforeX) > 1e-7 || std::fabs(s.cpv - beforeY) > 1e-7)
            moved++;
    }

    inFlightEdit = true;
    if (cb.modelChanged)
        cb.modelChanged(-1);
    inFlightEdit = false;

    if (cb.announce)
    {
        if (usable == 0)
            cb.announce(dx != 0 ? "No horizontal control points in selection"
                                : "No control points in selection");
        else if (moved > 0)
            cb.announce(fmt::format("{} control points moved", moved));
        else
            cb.announce("At limit");
    }

    return moved > 0;
}

void MSEGAccessibleKeyboardHandler::groupDelete(bool removeSegment)
{
    auto sel = cb.getSelection ? cb.getSelection() : std::vector<int>();

    // highest time first, so earlier node times stay valid as segments go away
    std::sort(sel.begin(), sel.end(), std::greater<int>());

    if (cb.prepareForUndo)
        cb.prepareForUndo();

    int deleted = 0;
    for (auto i : sel)
    {
        if (i <= 0 || i >= ms->n_activeSegments)
            continue;
        if (ms->n_activeSegments <= 1)
            break;

        if (removeSegment)
            Surge::MSEG::deleteSegment(ms, nodeTime(i));
        else
            Surge::MSEG::unsplitSegment(ms, nodeTime(i));
        Surge::MSEG::rebuildCache(ms);
        deleted++;
    }

    if (deleted == 0)
    {
        if (cb.announce)
            cb.announce("Nothing deleted, start node cannot be deleted");
        return;
    }

    if (cb.pushToUndo)
        cb.pushToUndo();

    clearSelection();

    inFlightEdit = true;
    if (cb.modelChanged)
        cb.modelChanged(-1);
    inFlightEdit = false;

    placeCursorOnNode(std::min(index, numNodes() - 1));
    if (cb.announce)
        cb.announce(fmt::format("{} nodes deleted. ", deleted) + nodeAnnouncement(index));
}

void MSEGAccessibleKeyboardHandler::cancelModes() { mode = Mode::CURSOR; }

void MSEGAccessibleKeyboardHandler::revalidateCursor(bool announceIfMoved)
{
    if (!ms || ms->n_activeSegments <= 0)
        return;

    auto N = ms->n_activeSegments;
    auto oldIndex = index;

    if (index > N)
    {
        // resolve to the node nearest where the cursor last was in time
        int best = 0;
        float bestDist = std::numeric_limits<float>::max();
        for (int i = 0; i <= N; ++i)
        {
            auto d = std::fabs(nodeTime(i) - rememberedTime);
            if (d < bestDist)
            {
                bestDist = d;
                best = i;
            }
        }
        index = best;
    }

    if (cb.getSelection && cb.setSelection)
    {
        auto sel = cb.getSelection();
        std::vector<int> pruned;
        for (auto si : sel)
            if (si >= 0 && si < N)
                pruned.push_back(si);

        if (pruned.size() != sel.size())
            cb.setSelection(pruned);
    }

    rememberedTime = nodeTime(index);

    if (index != oldIndex)
    {
        if (cb.repaint)
            cb.repaint();
        if (announceIfMoved)
            announceCursor();
    }
}

bool MSEGAccessibleKeyboardHandler::processKey(const juce::KeyPress &key)
{
    if (!ms || ms->n_activeSegments <= 0)
        return false;

    // never operate on a stale cursor
    revalidateCursor(false);

    if (mode == Mode::SELECTION)
        return processSelectionKey(key);
    return processCursorKey(key);
}

bool MSEGAccessibleKeyboardHandler::processCursorKey(const juce::KeyPress &key)
{
    auto mods = key.getModifiers();
    auto kc = key.getKeyCode();
    auto c = altLetter(key);
    auto N = ms->n_activeSegments;

    if (kc == juce::KeyPress::tabKey)
    {
        moveCursor(mods.isShiftDown() ? -1 : 1);
        return true;
    }

    if (kc == juce::KeyPress::leftKey || kc == juce::KeyPress::rightKey)
    {
        auto dx = (kc == juce::KeyPress::rightKey ? 1.f : -1.f) * xStep(mods);

        if (mods.isAltDown())
        {
            if (hasSelection())
            {
                if (cb.prepareForUndo)
                    cb.prepareForUndo();
                if (groupNudgeCP(dx, 0.f) && cb.pushToUndo)
                    cb.pushToUndo();
                return true;
            }

            auto seg = segmentForCursor();

            if (!controlPointUsable(seg))
            {
                if (cb.announce)
                    cb.announce("No control point on this segment");
                return true;
            }

            if (cb.prepareForUndo)
                cb.prepareForUndo();
            auto before = ms->segments[seg].cpduration;
            nudgeControlPoint(seg, dx, 0.f);
            if (std::fabs(ms->segments[seg].cpduration - before) > 1e-7 && cb.pushToUndo)
                cb.pushToUndo();
            return true;
        }

        if (hasSelection())
        {
            if (cb.prepareForUndo)
                cb.prepareForUndo();
            if (groupNudgeX(dx, xSnapFor(mods)) && cb.pushToUndo)
                cb.pushToUndo();
            return true;
        }

        if (cb.prepareForUndo)
            cb.prepareForUndo();
        auto before = nodeTime(index);
        nudgeNodeX(index, dx, xSnapFor(mods), true);
        if (std::fabs(nodeTime(index) - before) > 1e-7 && cb.pushToUndo)
            cb.pushToUndo();
        return true;
    }

    if (kc == juce::KeyPress::upKey || kc == juce::KeyPress::downKey)
    {
        auto dy = (kc == juce::KeyPress::upKey ? 1.f : -1.f) * yStep(mods);

        if (mods.isAltDown())
        {
            if (hasSelection())
            {
                if (cb.prepareForUndo)
                    cb.prepareForUndo();
                if (groupNudgeCP(0.f, dy) && cb.pushToUndo)
                    cb.pushToUndo();
                return true;
            }

            auto seg = segmentForCursor();

            if (!controlPointUsable(seg))
            {
                if (cb.announce)
                    cb.announce("No control point on this segment");
                return true;
            }

            if (cb.prepareForUndo)
                cb.prepareForUndo();
            auto before = ms->segments[seg].cpv;
            nudgeControlPoint(seg, 0.f, dy);
            if (std::fabs(ms->segments[seg].cpv - before) > 1e-7 && cb.pushToUndo)
                cb.pushToUndo();
            return true;
        }

        if (hasSelection())
        {
            if (cb.prepareForUndo)
                cb.prepareForUndo();
            if (groupNudgeY(dy, ySnapFor(mods)) && cb.pushToUndo)
                cb.pushToUndo();
            return true;
        }

        if (cb.prepareForUndo)
            cb.prepareForUndo();
        auto before = nodeValue(index);
        nudgeNodeY(index, dy, ySnapFor(mods), true);
        if (std::fabs(nodeValue(index) - before) > 1e-7 && cb.pushToUndo)
            cb.pushToUndo();
        return true;
    }

    if (kc == juce::KeyPress::homeKey || kc == juce::KeyPress::endKey)
    {
        auto pre = std::string(hasSelection() ? "Selection cleared. " : "");
        if (!pre.empty())
            clearSelection();

        placeCursorOnNode(kc == juce::KeyPress::homeKey ? 0 : numNodes() - 1);
        if (cb.announce)
            cb.announce(pre + nodeAnnouncement(index));
        return true;
    }

    if (kc == juce::KeyPress::pageUpKey || kc == juce::KeyPress::pageDownKey)
    {
        auto seg = segmentForCursor();
        auto target = seg + (kc == juce::KeyPress::pageDownKey ? 1 : -1);

        if (target < 0)
        {
            if (cb.announce)
                cb.announce("First segment");
            return true;
        }
        if (target >= N)
        {
            if (cb.announce)
                cb.announce("Last segment");
            return true;
        }

        auto pre = std::string(hasSelection() ? "Selection cleared. " : "");
        if (!pre.empty())
            clearSelection();

        placeCursorOnNode(target);
        if (cb.announce)
            cb.announce(pre + nodeAnnouncement(index));
        return true;
    }

    if (kc == juce::KeyPress::returnKey)
    {
        if (mods.isAltDown())
        {
            auto seg = segmentForCursor();

            if (!controlPointUsable(seg))
            {
                if (cb.announce)
                    cb.announce("No control point on this segment");
                return true;
            }

            if (cb.showTypein)
                cb.showTypein(seg, controlPointIs2D(seg) ? kTypein2DChooser : kTypeinCPValue);
            return true;
        }

        if (index >= N)
        {
            if (ms->endpointMode == MSEGStorage::EndpointMode::LOCKED)
            {
                if (cb.announce)
                    cb.announce("Editing start node value, linked endpoints");
                if (cb.showTypein)
                    cb.showTypein(0, kTypeinValue);
            }
            else if (cb.announce)
            {
                cb.announce("No type-in for the final node, use arrow keys");
            }
        }
        else if (cb.showTypein)
        {
            cb.showTypein(index, kTypeinValue);
        }
        return true;
    }

    if (kc == juce::KeyPress::deleteKey || kc == juce::KeyPress::backspaceKey)
    {
        if (hasSelection())
            groupDelete(mods.isShiftDown());
        else
            deleteNode(mods.isShiftDown());
        return true;
    }

    if ((kc == juce::KeyPress::F10Key && mods.isShiftDown()) || kc == 93 ||
        key.getTextCharacter() == ']')
    {
        if (cb.openContextMenuForSegment)
            cb.openContextMenuForSegment(segmentForCursor());
        return true;
    }

    if (mods.isCommandDown() && !mods.isAltDown() && lowerChar(key) == 'a')
    {
        selectAll();
        return true;
    }

    if (c == 'a')
    {
        if (hasSelection())
        {
            if (cb.announce)
                cb.announce("Clear the selection first to add a node");
            return true;
        }
        addNode(mods.isShiftDown());
        return true;
    }

    if (c == 'r')
    {
        resetCP();
        return true;
    }

    if (c == 's')
    {
        enterSelection();
        return true;
    }

    if (c == 'i')
    {
        announceEditorState();
        return true;
    }

    return false;
}

bool MSEGAccessibleKeyboardHandler::processSelectionKey(const juce::KeyPress &key)
{
    auto mods = key.getModifiers();
    auto kc = key.getKeyCode();
    auto N = selectableNodeCount();

    if (kc == juce::KeyPress::returnKey || kc == juce::KeyPress::escapeKey || altLetter(key) == 's')
    {
        exitSelection();
        return true;
    }

    if (kc == juce::KeyPress::leftKey || kc == juce::KeyPress::rightKey)
    {
        auto target = index + (kc == juce::KeyPress::rightKey ? 1 : -1);

        if (target < 0 || target >= N)
        {
            if (cb.announce)
                cb.announce(target < 0 ? "First node" : "Last node");
            return true;
        }

        // the cursor is the active end of the range; moving away from the
        // anchor grows the selection, moving back toward it shrinks it
        bool growing = std::abs(target - selectionAnchor) > std::abs(index - selectionAnchor);
        auto unselected = index;

        placeCursorOnNode(target);
        applyAnchorRange();

        if (cb.announce)
        {
            auto nSel = std::abs(index - selectionAnchor) + 1;
            if (growing)
                cb.announce(fmt::format("Node {} selected, {} nodes selected", index + 1, nSel));
            else
                cb.announce(
                    fmt::format("Node {} unselected, {} nodes selected", unselected + 1, nSel));
        }
        return true;
    }

    if (mods.isCommandDown() && !mods.isAltDown() && lowerChar(key) == 'a')
    {
        selectAll();
        return true;
    }

    // consume Tab so focus traversal can't fire mid-selection
    if (kc == juce::KeyPress::tabKey)
        return true;

    return false;
}

} // namespace Overlays
} // namespace Surge

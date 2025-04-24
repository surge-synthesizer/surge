/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
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

#include "MSEGEditor.h"
#include "MSEGModulationHelper.h"
#include "SurgeGUIUtils.h"
#include "DebugHelpers.h"
#include "SkinColors.h"
#include "basic_dsp.h" // for limit_range
#include "SurgeImage.h"
#include "SurgeImageStore.h"
#include "SurgeGUIEditor.h"
#include "RuntimeFont.h"

#include "widgets/MultiSwitch.h"
#include "widgets/NumberField.h"
#include "widgets/Switch.h"
#include "overlays/TypeinParamEditor.h"
#include <set>
#include "widgets/MenuCustomComponents.h"

namespace Surge
{
namespace Overlays
{

struct TimeThisBlock
{
    TimeThisBlock(const std::string &tag) : tag(tag)
    {
        start = std::chrono::high_resolution_clock::now();
    }
    void bump(const std::string &msg)
    {
        auto end = std::chrono::high_resolution_clock::now();
        auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "   " << msg << tag << " @ " << int_ms.count() << std::endl;
    }
    ~TimeThisBlock()
    {
        auto end = std::chrono::high_resolution_clock::now();
        auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Block in " << tag << " took " << int_ms.count() << "ms ("
                  << 1000.0 / int_ms.count() << " fps)" << std::endl;
    }
    std::string tag;
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
};

struct MSEGCanvas;

struct MSEGControlRegion : public juce::Component,
                           public Surge::GUI::SkinConsumingComponent,
                           public Surge::GUI::IComponentTagValue::Listener
{
    MSEGControlRegion(MSEGCanvas *c, SurgeStorage *storage, LFOStorage *lfos, MSEGStorage *ms,
                      MSEGEditor::State *eds, Surge::GUI::Skin::ptr_t skin,
                      std::shared_ptr<SurgeImageStore> b, SurgeGUIEditor *sge)
        : juce::Component("MSEG Control Region")
    {
        setSkin(skin, b);
        this->ms = ms;
        this->eds = eds;
        this->lfodata = lfos;
        this->canvas = c;
        this->storage = storage;
        this->sge = sge;
        setAccessible(true);
        setTitle("Controls");
        setDescription("Controls");
        setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        rebuild();
    };

    enum ControlTags
    {
        tag_segment_nodeedit_mode = 1231231, // Just to push outside any ID range
        tag_segment_movement_mode,
        tag_vertical_snap,
        tag_vertical_value,
        tag_horizontal_snap,
        tag_horizontal_value,
        tag_loop_mode,
        tag_edit_mode,
    };

    std::unique_ptr<Surge::Overlays::TypeinLambdaEditor> typeinEditor;

    void rebuild();
    void valueChanged(Surge::GUI::IComponentTagValue *p) override;
    int32_t controlModifierClicked(Surge::GUI::IComponentTagValue *pControl,
                                   const juce::ModifierKeys &button,
                                   bool isDoubleClickEvent) override;

    void paint(juce::Graphics &g) override { g.fillAll(skin->getColor(Colors::MSEGEditor::Panel)); }

    std::unique_ptr<Surge::Widgets::Switch> hSnapButton, vSnapButton;
    std::unique_ptr<Surge::Widgets::MultiSwitch> loopMode, editMode, movementMode;
    std::unique_ptr<Surge::Widgets::NumberField> hSnapSize, vSnapSize;
    std::vector<std::unique_ptr<juce::Label>> labels;

    void hvSnapTypein(bool isH);

    MSEGStorage *ms = nullptr;
    MSEGEditor::State *eds = nullptr;
    MSEGCanvas *canvas = nullptr;
    LFOStorage *lfodata = nullptr;
    SurgeStorage *storage = nullptr;
    SurgeGUIEditor *sge = nullptr;
};

struct MSEGCanvas : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
    MSEGCanvas(SurgeStorage *storage, LFOStorage *lfodata, MSEGStorage *ms, MSEGEditor::State *eds,
               Surge::GUI::Skin::ptr_t skin, std::shared_ptr<SurgeImageStore> b,
               SurgeGUIEditor *sge)
        : juce::Component("MSEG Canvas")
    {
        setSkin(skin, b);
        this->storage = storage;
        this->ms = ms;
        this->eds = eds;
        this->lfodata = lfodata;
        this->sge = sge;
        Surge::MSEG::rebuildCache(ms);
        handleDrawable = b->getImage(IDB_MSEG_NODES);
        timeEditMode = (MSEGCanvas::TimeEdit)eds->timeEditMode;
        setOpaque(true);
        setTitle("MSEG Display and Edit Canvas");
        setDescription("MSEG Display and Edit Canvas");
    };

    enum SegmentProps
    {
        duration,
        value,
        cp_duration,
        cp_value,
    };

    std::unique_ptr<Surge::Overlays::TypeinLambdaEditor> typeinEditor;

    /*
    ** We make a list of hotzones when we draw so we don't have to recalculate the
    ** mouse locations in drag and so on events
    */
    struct hotzone
    {
        juce::Rectangle<float> rect;
        juce::Rectangle<float> drawRect;
        bool useDrawRect = false;
        int associatedSegment;
        bool active = false;
        bool dragging = false;
        // For pan and zoom we need to treat the endpoint of the last segment specially
        bool specialEndpoint = false;

        enum Type
        {
            MOUSABLE_NODE,
            INACTIVE_NODE, // To keep the array the same size add dummies when you suppress controls
            LOOPMARKER
        } type;

        enum ZoneSubType
        {
            SEGMENT_ENDPOINT,
            SEGMENT_CONTROL,
            LOOP_START,
            LOOP_END
        } zoneSubType = SEGMENT_CONTROL;

        enum SegmentControlDirection
        {
            VERTICAL_ONLY = 1,
            HORIZONTAL_ONLY,
            BOTH_DIRECTIONS
        } segmentDirection = VERTICAL_ONLY;
        std::function<void(float, float, const juce::Point<float> &)> onDrag;
    };

    std::function<void()> onModelChanged = []() {};
    std::vector<hotzone> hotzones;

    static constexpr int drawInsertX = 10, drawInsertY = 10, axisSpaceX = 18, axisSpaceY = 8;

    inline float drawDuration()
    {
        /*
        if( ms->n_activeSegments == 0 )
           return 1.0;
        return std::max( 1.0f, ms->totalDuration );
         */
        return ms->axisWidth;
    }

    inline juce::Rectangle<int> getDrawArea()
    {
        auto drawArea = getLocalBounds()
                            .reduced(drawInsertX, drawInsertY)
                            .withTrimmedBottom(axisSpaceY)
                            .withTrimmedLeft(axisSpaceX)
                            .withTrimmedTop(4);
        return drawArea;
    }

    inline juce::Rectangle<int> getHAxisArea()
    {
        auto drawArea = getLocalBounds().reduced(drawInsertX, drawInsertY);
        drawArea = drawArea.withTop(drawArea.getBottom() - axisSpaceY).withTrimmedLeft(axisSpaceX);
        return drawArea;
    }

    // Stretch all the way to the bottom
    inline juce::Rectangle<int> getHAxisDoubleClickArea()
    {
        return getHAxisArea().withBottom(getLocalBounds().getBottom());
    }

    inline juce::Rectangle<int> getVAxisArea()
    {
        auto drawArea = getLocalBounds()
                            .reduced(drawInsertX, drawInsertY)
                            .withWidth(axisSpaceX)
                            .withTrimmedBottom(axisSpaceY);
        return drawArea;
    }

    std::function<float(float)> valToPx()
    {
        auto drawArea = getDrawArea();
        float vscale = drawArea.getHeight();
        return [vscale, drawArea](float vp) -> float {
            float v = 1 - (vp + 1) * 0.5;
            return v * vscale + drawArea.getY();
        };
    }

    std::function<float(float)> pxToVal()
    {
        auto drawArea = getDrawArea();
        float vscale = drawArea.getHeight();
        return [vscale, drawArea](float vx) -> float {
            auto v = (vx - drawArea.getY()) / vscale;
            auto vp = (1 - v) * 2 - 1;
            return vp;
        };
    }

    std::function<float(float)> timeToPx()
    {
        auto drawArea = getDrawArea();
        float maxt = drawDuration();
        float tscale = 1.f * drawArea.getWidth() / maxt;
        return [tscale, drawArea, this](float t) {
            return (t - ms->axisStart) * tscale + drawArea.getX();
        };
    }

    std::function<float(float)> pxToTime() // INVESTIGATE
    {
        auto drawArea = getDrawArea();
        float maxt = drawDuration();
        float tscale = 1.f * drawArea.getWidth() / maxt;

        // So px = t * tscale + drawarea;
        // So t = ( px - drawarea ) / tscale;
        return [tscale, drawArea, this](float px) {
            return (px - drawArea.getX()) / tscale + ms->axisStart;
        };
    }

    void offsetValue(float &v, float d) { v = limit_range(v + d, -1.f, 1.f); }

    void adjustValue(int idx, bool cpvNotV0, float d, float snapResolution)
    {
        if (cpvNotV0)
        {
            offsetValue(ms->segments[idx].cpv, d);
        }
        else
        {
            offsetValue(ms->segments[idx].dragv0, d);
            if (snapResolution <= 0)
                ms->segments[idx].v0 = ms->segments[idx].dragv0;
            else
            {
                float q = ms->segments[idx].dragv0 + 1;
                float pos = round(q / snapResolution) * snapResolution;
                float adj = pos - 1.0;
                ms->segments[idx].v0 = limit_range(adj, -1.f, 1.f);
            }
        }
    }

    /*
     * This little struct acts as a SmapGuard so that shift drags can reset snap and
     * unreset snap
     */
    struct SnapGuard
    {
        SnapGuard(MSEGCanvas *c) : c(c)
        {
            hSnapO = c->ms->hSnap;
            vSnapO = c->ms->vSnap;
            c->repaint();
        }
        ~SnapGuard()
        {
            c->ms->hSnap = hSnapO;
            c->ms->vSnap = vSnapO;
            c->repaint();
        }
        MSEGCanvas *c;
        float hSnapO, vSnapO;
    };
    std::shared_ptr<SnapGuard> snapGuard;

    enum TimeEdit
    {
        SINGLE, // movement bound between two neighboring nodes
        SHIFT,  // shifts all following nodes along, relatively
        DRAW,   // only change amplitude of nodes as the cursor passes along the timeline
    } timeEditMode = SINGLE;

    const int loopMarkerWidth = 7, loopMarkerHeight = 15;

    void recalcHotZones(const juce::Point<int> &where)
    {
        hotzones.clear();

        auto drawArea = getDrawArea();

        auto handleRadius = 6.5;

        float maxt = drawDuration();
        float tscale = 1.f * drawArea.getWidth() / maxt;
        float vscale = drawArea.getHeight();
        auto valpx = valToPx();
        auto tpx = timeToPx();
        auto pxt = pxToTime();

        // Put in the loop marker boxes
        if (ms->loopMode != MSEGStorage::LoopMode::ONESHOT && ms->editMode != MSEGStorage::LFO)
        {
            int ls = (ms->loop_start >= 0 ? ms->loop_start : 0);
            int le = (ms->loop_end >= 0 ? ms->loop_end : ms->n_activeSegments - 1);
            float pxs = tpx(ms->segmentStart[ls]);
            float pxe = tpx(ms->segmentEnd[le]);
            auto hs = hotzone();
            hs.type = hotzone::Type::LOOPMARKER;
            hs.segmentDirection = hotzone::HORIZONTAL_ONLY;
            hs.associatedSegment = -1;

            auto he = hs;

            auto haxisArea = getHAxisArea();

            if (this->ms->editMode != MSEGStorage::LFO)
            {
                hs.onDrag = [pxt, this](float x, float y, const juce::Point<float> &w) {
                    auto t = pxt(w.x);
                    t = limit_range(t, 0.f, ms->segmentStart[ms->n_activeSegments - 1]);

                    auto seg = Surge::MSEG::timeToSegment(this->ms, t);

                    float pct = 0;
                    if (this->ms->segments[seg].duration > 0)
                    {
                        pct = (t - this->ms->segmentStart[seg]) / this->ms->segments[seg].duration;
                    }
                    if (pct > 0.5)
                    {
                        seg++;
                    }
                    if (seg != ms->loop_start)
                    {
                        Surge::MSEG::setLoopStart(ms, seg);
                        modelChanged();
                        repaint();
                    }
                    loopDragTime = t;
                    loopDragIsStart = true;
                    if (ms->loop_start >= 0)
                        loopDragEnd = this->ms->segmentStart[ms->loop_start];
                    else
                        loopDragEnd = 0;
                };

                hs.rect = juce::Rectangle<float>(pxs - 0.5, haxisArea.getY() + 1, loopMarkerWidth,
                                                 loopMarkerHeight);
                hs.zoneSubType = hotzone::LOOP_START;

                he.rect = juce::Rectangle<float>(pxe - loopMarkerWidth + 0.5, haxisArea.getY() + 1,
                                                 loopMarkerWidth, loopMarkerHeight);
                he.zoneSubType = hotzone::LOOP_END;

                he.onDrag = [pxt, this](float x, float y, const juce::Point<float> &w) {
                    auto t = pxt(w.x);
                    t = limit_range(t, ms->segmentEnd[0], ms->totalDuration);
                    auto seg = Surge::MSEG::timeToSegment(this->ms, t);
                    if (t == ms->totalDuration)
                        seg = ms->n_activeSegments - 1;
                    if (seg > 0)
                        seg--; // since this is the END marker
                    float pct = 0;
                    if (this->ms->segments[seg + 1].duration > 0)
                    {
                        pct =
                            (t - this->ms->segmentEnd[seg]) / this->ms->segments[seg + 1].duration;
                    }
                    if (pct > 0.5)
                    {
                        seg++;
                    }
                    if (seg != ms->loop_end)
                    {
                        Surge::MSEG::setLoopEnd(ms, seg);
                        modelChanged();
                        repaint();
                    }
                    loopDragTime = t;
                    loopDragIsStart = false;
                    if (ms->loop_end >= 0)
                        loopDragEnd = this->ms->segmentEnd[ms->loop_end];
                    else
                        loopDragEnd = this->ms->totalDuration;
                };

                hotzones.push_back(hs);
                hotzones.push_back(he);
            }
        }

        for (int i = 0; i < ms->n_activeSegments; ++i)
        {
            auto t0 = tpx(ms->segmentStart[i]);
            auto t1 = tpx(ms->segmentEnd[i]);

            auto segrec = juce::Rectangle<float>(juce::Point<float>(t0, drawArea.getY()),
                                                 juce::Point<float>(t1, drawArea.getBottom()));

            // Now add the mousable zones
            auto &s = ms->segments[i];
            auto rectForPoint =
                [&](float t, float v, hotzone::ZoneSubType mt,
                    std::function<void(float, float, const juce::Point<float> &)> onDrag) {
                    auto h = hotzone();
                    h.rect = juce::Rectangle<float>(
                        juce::Point<float>(t - handleRadius, valpx(v) - handleRadius),
                        juce::Point<float>(t + handleRadius, valpx(v) + handleRadius));
                    h.type = hotzone::Type::MOUSABLE_NODE;
                    if (h.rect.contains(where.getX(), where.getY()))
                        h.active = true;
                    h.onDrag = onDrag;
                    h.associatedSegment = i;
                    h.zoneSubType = mt;
                    hotzones.push_back(h);
                };

            auto timeConstraint = [&](int prior, float dx) {
                switch (this->timeEditMode)
                {
                case DRAW:
                    break;
                case SHIFT:
                    if (lassoSelector)
                    {
                        bool isLast{true};
                        for (auto si : lassoSelector->items)
                        {
                            isLast = isLast && si <= prior + 1;
                        }
                        if (isLast)
                        {
                            Surge::MSEG::adjustDurationShiftingSubsequent(this->ms, prior, dx,
                                                                          ms->hSnap, longestMSEG);
                        }
                        else
                        {
                            Surge::MSEG::adjustDurationConstantTotalDuration(this->ms, prior, dx,
                                                                             ms->hSnap);
                        }
                    }
                    else
                    {
                        Surge::MSEG::adjustDurationShiftingSubsequent(this->ms, prior, dx,
                                                                      ms->hSnap, longestMSEG);
                    }
                    break;
                case SINGLE:
                    Surge::MSEG::adjustDurationConstantTotalDuration(this->ms, prior, dx,
                                                                     ms->hSnap);
                    break;
                }
            };

            auto unipolarFactor = 1 + lfodata->unipolar.val.b;

            // We get a mousable point at the start of the line
            rectForPoint(t0, s.v0, hotzone::SEGMENT_ENDPOINT,
                         [i, this, vscale, tscale, timeConstraint,
                          unipolarFactor](float dx, float dy, const juce::Point<float> &where) {
                             adjustValue(i, false, -2 * dy / vscale, ms->vSnap * unipolarFactor);

                             if (i != 0)
                             {
                                 timeConstraint(i - 1, dx / tscale);
                             }
                         });

            // Control point editor
            if (ms->segments[i].duration > 0.01 &&
                ms->segments[i].type != MSEGStorage::segment::HOLD)
            {
                // Drop in a control point. But where and moving how?
                // Here's some good defaults
                bool verticalMotion = true;
                bool horizontalMotion = false;
                bool verticalScaleByValues = false;

                switch (ms->segments[i].type)
                {
                // Ones where we scan the entire square
                case MSEGStorage::segment::QUAD_BEZIER:
                case MSEGStorage::segment::BROWNIAN:
                    horizontalMotion = true;
                    break;
                // Ones where we stay within the range
                case MSEGStorage::segment::LINEAR:
                    verticalScaleByValues = true;
                    break;
                default:
                    break;
                }

                // Follow the mouse with scaling of course

                float vLocation = ms->segments[i].cpv;
                if (verticalScaleByValues)
                    vLocation = 0.5 * (vLocation + 1) * (ms->segments[i].nv1 - ms->segments[i].v0) +
                                ms->segments[i].v0;

                // switch is here in case some other curves need the fixed control point in other
                // places horizontally
                float fixtLocation;
                switch (ms->segments[i].type)
                {
                default:
                    fixtLocation = 0.5;
                    break;
                }

                float tLocation = fixtLocation * ms->segments[i].duration + ms->segmentStart[i];

                if (horizontalMotion)
                    tLocation =
                        ms->segments[i].cpduration * ms->segments[i].duration + ms->segmentStart[i];

                float t = tpx(tLocation);
                float v = valpx(vLocation);
                auto h = hotzone();

                h.rect =
                    juce::Rectangle<float>(juce::Point<float>(t - handleRadius, v - handleRadius),
                                           juce::Point<float>(t + handleRadius, v + handleRadius));
                h.type = hotzone::Type::MOUSABLE_NODE;

                if (h.rect.contains(where.getX(), where.getY()))
                {
                    h.active = true;
                }

                h.useDrawRect = false;

                if ((verticalMotion && !horizontalMotion &&
                     (ms->segments[i].type != MSEGStorage::segment::LINEAR &&
                      ms->segments[i].type != MSEGStorage::segment::BUMP)) ||
                    ms->segments[i].type == MSEGStorage::segment::BROWNIAN)
                {
                    float t = tpx(0.5 * ms->segments[i].duration + ms->segmentStart[i]);
                    float v = valpx(0.5 * (ms->segments[i].nv1 + ms->segments[i].v0));

                    h.drawRect = juce::Rectangle<float>(
                        juce::Point<float>(t - handleRadius, v - handleRadius),
                        juce::Point<float>(t + handleRadius, v + handleRadius));
                    h.rect = h.drawRect;
                    h.useDrawRect = true;
                }

                float segdt = ms->segmentEnd[i] - ms->segmentStart[i];
                float segdx = ms->segments[i].nv1 - ms->segments[i].v0;

                h.onDrag = [this, i, tscale, vscale, verticalMotion, horizontalMotion,
                            verticalScaleByValues, segdt,
                            segdx](float dx, float dy, const juce::Point<float> &where) {
                    if (verticalMotion)
                    {
                        float dv = 0;

                        if (verticalScaleByValues)
                        {
                            if (segdx == 0)
                                dv = 0;
                            else
                                dv = -2 * dy / vscale / (0.5 * segdx);
                        }
                        else
                            dv = -2 * dy / vscale;

                        /*
                         * OK we have some special case edge scalings for sensitive types.
                         * If this ends up applying to more than linear then I would be surprised
                         * but write a switch anyway :)
                         */
                        switch (ms->segments[i].type)
                        {
                        case MSEGStorage::segment::LINEAR:
                        case MSEGStorage::segment::SCURVE:
                        {
                            // Slowdown parameters. Basically slow down mouse -> delta linearly near
                            // edge
                            float slowdownInTheLast = 0.15;
                            float slowdownAtMostTo = 0.015;
                            float adj = 1;

                            if (ms->segments[i].cpv > (1.0 - slowdownInTheLast))
                            {
                                auto lin = (ms->segments[i].cpv - slowdownInTheLast) /
                                           (1.0 - slowdownInTheLast);
                                adj = (1.0 - (1.0 - slowdownAtMostTo) * lin);
                            }
                            else if (ms->segments[i].cpv < (-1.0 + slowdownInTheLast))
                            {
                                auto lin = (ms->segments[i].cpv + slowdownInTheLast) /
                                           (-1.0 + slowdownInTheLast);
                                adj = (1.0 - (1.0 - slowdownAtMostTo) * lin);
                            }
                            dv *= adj;
                        }
                        default:
                            break;
                        }

                        ms->segments[i].cpv += dv;
                    }

                    if (horizontalMotion)
                    {
                        ms->segments[i].cpduration += dx / tscale / segdt;
                    }

                    Surge::MSEG::constrainControlPointAt(ms, i);
                    modelChanged(i);
                };

                h.associatedSegment = i;
                h.zoneSubType = hotzone::SEGMENT_CONTROL;

                if (verticalMotion && horizontalMotion)
                    h.segmentDirection = hotzone::BOTH_DIRECTIONS;
                else if (horizontalMotion)
                    h.segmentDirection = hotzone::HORIZONTAL_ONLY;
                else
                    h.segmentDirection = hotzone::VERTICAL_ONLY;

                hotzones.push_back(h);
            }
            else
            {
                hotzone h;
                h.type = hotzone::INACTIVE_NODE;
                hotzones.push_back(h);
            }

            // Specially we have to add an endpoint editor
            if (i == ms->n_activeSegments - 1)
            {
                rectForPoint(
                    tpx(ms->totalDuration),
                    ms->segments[ms->n_activeSegments - 1].nv1, // which is [0].v0 in lock mode only
                    hotzone::SEGMENT_ENDPOINT,
                    [this, vscale, tscale, unipolarFactor](float dx, float dy,
                                                           const juce::Point<float> &where) {
                        if (ms->endpointMode == MSEGStorage::EndpointMode::FREE)
                        {
                            float d = -2 * dy / vscale;
                            float snapResolution = ms->vSnap * unipolarFactor;
                            int idx = ms->n_activeSegments - 1;

                            offsetValue(ms->segments[idx].dragv1, d);

                            if (snapResolution <= 0)
                                ms->segments[idx].nv1 = ms->segments[idx].dragv1;
                            else
                            {
                                float q = ms->segments[idx].dragv1 + 1;
                                float pos = round(q / snapResolution) * snapResolution;
                                float adj = pos - 1.0;

                                ms->segments[idx].nv1 = limit_range(adj, -1.f, 1.f);
                            }
                        }
                        else
                        {
                            adjustValue(0, false, -2 * dy / vscale, ms->vSnap);
                        }

                        // We need to deal with the cpduration also
                        auto cpv = this->ms->segments[ms->n_activeSegments - 1].cpduration /
                                   this->ms->segments[ms->n_activeSegments - 1].duration;

                        // Don't allow endpoint time adjust in LFO mode
                        if (this->ms->editMode == MSEGStorage::ENVELOPE)
                        {
                            if (!this->getLocalBounds().contains(where.toInt()))
                            {
                                auto howFar = where.x - this->getLocalBounds().getRight();

                                if (howFar > 0)
                                    dx *= howFar * 0.1; // this is really just a speedup as our axes
                                                        // shrinks. Just a fudge
                            }

                            Surge::MSEG::adjustDurationShiftingSubsequent(
                                ms, ms->n_activeSegments - 1, dx / tscale, ms->hSnap, longestMSEG);
                        }
                    });

                hotzones.back().specialEndpoint = true;
            }
        }
    }

    // vertical grid thinning
    const int gridMaxVSteps = 10;

    void drawLoopDragMarker(juce::Graphics &g, juce::Colour markerColor,
                            hotzone::ZoneSubType subtype, const juce::Rectangle<float> &rect)
    {
        auto ha = getHAxisArea();
        auto height = rect.getHeight();
        auto start = rect.getRight();
        auto end = rect.getX();
        auto top = rect.getY();

        if (subtype == hotzone::LOOP_START)
        {
            std::swap(start, end);
        }

        juce::Path p;
        p.startNewSubPath(start, top);
        p.lineTo(start, top + height);
        p.lineTo(end, top + height);
        p.closeSubPath();

        g.setColour(markerColor);
        g.fillPath(p);

        g.drawLine(start, top, start, top + height, 1);
    }

    void drawLoopDragMarker(juce::Graphics &g, juce::Colour markerColor,
                            hotzone::ZoneSubType subtype, float time)
    {
        auto tpx = timeToPx();
        auto ha = getHAxisArea();
        auto start = tpx(time) - 0.5;
        bool hide = false;

        if (subtype == hotzone::LOOP_END)
        {
            start -= loopMarkerWidth;

            /* this -1 is because we offset half a px to the left in our rect */
            if (start < ha.getX() - 1 || start + loopMarkerWidth > ha.getRight())
            {
                hide = true;
            }
        }
        else
        {
            /* this -1 is because we offset half a px to the left in our rect */
            if (start < ha.getX() - 1 || start + loopMarkerWidth > ha.getRight())
            {
                hide = true;
            }
        }

        if (!hide)
        {
            drawLoopDragMarker(
                g, markerColor, subtype,
                juce::Rectangle<float>(start, ha.getY(), loopMarkerWidth, loopMarkerHeight));
        }
    }

    inline void drawAxis(juce::Graphics &g)
    {
        auto primaryFont = skin->fontManager->getLatoAtSize(9, juce::Font::bold);
        auto secondaryFont = skin->fontManager->getLatoAtSize(7);

        auto haxisArea = getHAxisArea();
        auto tpx = timeToPx();
        float maxt = drawDuration();

        // draw loop area
        if (ms->loopMode != MSEGStorage::ONESHOT && ms->editMode != MSEGStorage::LFO)
        {
            int ls = (ms->loop_start >= 0 ? ms->loop_start : 0);
            int le = (ms->loop_end >= 0 ? ms->loop_end : ms->n_activeSegments - 1);

            float pxs = limit_range((float)tpx(ms->segmentStart[ls]), (float)haxisArea.getX(),
                                    (float)haxisArea.getRight());
            float pxe = limit_range((float)tpx(ms->segmentEnd[le]), (float)haxisArea.getX(),
                                    (float)haxisArea.getRight());

            auto r = juce::Rectangle<float>(juce::Point<float>(pxs, haxisArea.getY()),
                                            juce::Point<float>(pxe, haxisArea.getY() + 15));

            // draw the loop region start to end
            if (!(ms->loop_start == ms->loop_end + 1))
            {
                g.setColour(skin->getColor(Colors::MSEGEditor::Loop::RegionAxis));
                g.fillRect(r);
            }

            auto mcolor = skin->getColor(Colors::MSEGEditor::Loop::Marker);

            // draw loop markers
            if ((loopDragTime < 0 || !loopDragIsStart) && (ls >= 0 && ls < ms->segmentStart.size()))
            {
                drawLoopDragMarker(g, mcolor, hotzone::LOOP_START, ms->segmentStart[ls]);
            }

            if ((loopDragTime < 0 || loopDragIsStart) && (le >= 0 && le < ms->segmentStart.size()))
            {
                drawLoopDragMarker(g, mcolor, hotzone::LOOP_END, ms->segmentEnd[le]);
            }

            // loop marker when dragged
            if (loopDragTime >= 0)
            {
                drawLoopDragMarker(g, mcolor,
                                   (loopDragIsStart ? hotzone::LOOP_START : hotzone::LOOP_END),
                                   loopDragTime);
            }
        }

        updateHTicks();

        for (auto hp : hTicks)
        {
            float t = hp.first;
            auto c = hp.second;
            float px = tpx(t);
            int yofs = 13;
            std::string txt;

            if (!(c & TickDrawStyle::kNoLabel))
            {

                int sw = 0;

                if (fmod(t, 1.f) == 0.f)
                {
                    g.setColour(skin->getColor(Colors::MSEGEditor::Axis::Text));
                    g.setFont(primaryFont);
                    txt = fmt::format("{:d}", int(t));
                    sw = SST_STRING_WIDTH_INT(primaryFont, txt);
                }
                else
                {
                    g.setColour(skin->getColor(Colors::MSEGEditor::Axis::SecondaryText));
                    g.setFont(secondaryFont);
                    txt = fmt::format("{:5.2f}", t);
                    sw = SST_STRING_WIDTH_INT(secondaryFont, txt);
                }

                g.drawText(txt, px - (sw / 2), haxisArea.getY() + 2, sw, yofs,
                           juce::Justification::centredBottom);
            }
        }

        // vertical axis
        auto vaxisArea = getVAxisArea();
        auto valpx = valToPx();

        vaxisArea = vaxisArea.expanded(1, 0);

        g.setColour(skin->getColor(Colors::MSEGEditor::Axis::Line));
        auto s = vaxisArea.getTopRight();
        auto b = vaxisArea.getBottomRight().translated(0, 4);
        g.drawLine(s.getX(), s.getY(), b.getX(), b.getY(), 1);

        g.setFont(primaryFont);
        g.setColour(skin->getColor(Colors::MSEGEditor::Axis::Text));

        updateVTicks();

        auto uni = lfodata->unipolar.val.b;

        for (auto vp : vTicks)
        {
            float p = valpx(std::get<0>(vp));

            float off = vaxisArea.getWidth() * 0.666;

            if (std::get<2>(vp))
            {
                off = 0;
            }

            if (off == 0)
            {
                g.setColour(skin->getColor(Colors::MSEGEditor::Axis::Line));
            }
            else
            {
                g.setColour(skin->getColor(Colors::MSEGEditor::Grid::SecondaryHorizontal));
                g.drawLine(vaxisArea.getX() + off, p, vaxisArea.getRight() - 1.f, p);
            }

            if (off == 0)
            {
                std::string txt;
                auto value = std::get<1>(vp);
                int ypos = 3;

                if (value == 0.f && std::signbit(value))
                {
                    value = -value;
                }

                txt = fmt::format("{:5.1f}", value);

                g.setColour(skin->getColor(Colors::MSEGEditor::Axis::SecondaryText));

                if (value == 1.f)
                {
                    g.drawText(txt, vaxisArea.getX() - 3, p, vaxisArea.getWidth(), 12,
                               juce::Justification::topRight);
                }
                else if (value == -1.f || (value == 0.f && uni))
                {
                    g.drawText(txt, vaxisArea.getX() - 3, p - 12, vaxisArea.getWidth(), 12,
                               juce::Justification::bottomRight);
                }
                else
                {
                    g.drawText(txt, vaxisArea.getX() - 3, p - 6, vaxisArea.getWidth(), 12,
                               juce::Justification::centredRight);
                }
            }
        }
    }

    enum TickDrawStyle
    {
        kNoLabel = 1 << 0,
        kHighlight = 1 << 1
    };

    std::vector<std::pair<float, int>> hTicks;
    float hTicksAsOf[3] = {-1, -1, -1};
    void updateHTicks()
    {
        if (hTicksAsOf[0] == ms->hSnapDefault && hTicksAsOf[1] == ms->axisStart &&
            hTicksAsOf[2] == ms->axisWidth)
            return;

        hTicksAsOf[0] = ms->hSnapDefault;
        hTicksAsOf[1] = ms->axisStart;
        hTicksAsOf[2] = ms->axisWidth;

        hTicks.clear();

        float dStep = ms->hSnapDefault;
        if (dStep <= 0)
        {
            /*
             * This error condition should never occur, but once I screwed up streaming, it did
             * and then this code runs forever so...
             */
            dStep = 0.01;
        }
        /*
         * OK two cases - step makes a squillion white lines, or step makes too few lines. Both of
         * these depend on this ratio
         */
        float widthByStep = ms->axisWidth / dStep;

        if (widthByStep < 4)
        {
            while (widthByStep < 4)
            {
                dStep /= 2;
                widthByStep = ms->axisWidth / dStep;
            }
        }
        else if (widthByStep > 20)
        {
            while (widthByStep > 20)
            {
                dStep *= 2;
                widthByStep = ms->axisWidth / dStep;
            }
        }

        // OK so what's our zero point.
        int startPoint = ceil(ms->axisStart / dStep);
        int endPoint = ceil((ms->axisStart + ms->axisWidth) / dStep);

        for (int i = startPoint; i <= endPoint; ++i)
        {
            float f = i * dStep;
            bool isInt = fabs(f - round(f)) < 1e-4;

            hTicks.push_back(std::make_pair(f, isInt ? kHighlight : 0));
        }
    }

    std::vector<std::tuple<float, float, bool>> vTicks; // position, display, is highlight
    float vTickAsOf = -1;
    void updateVTicks()
    {
        auto uni = lfodata->unipolar.val.b;

        if (ms->vSnapDefault != vTickAsOf)
        {
            vTicks.clear();

            float dStep = ms->vSnapDefault;
            // See comment above
            if (dStep <= 0)
            {
                dStep = 0.01;
            }

            while (dStep < 1.0 / (gridMaxVSteps * (1 + uni)))
            {
                dStep *= 2;
            }

            int steps = ceil(1.0 / dStep);
            int start, end;

            if (uni)
            {
                start = 0;
                end = steps + 1;
            }
            else
            {
                start = -steps - 1;
                end = steps + 1;
            }

            for (int vi = start; vi < end; vi++)
            {
                float val = vi * dStep;
                auto doDraw = true;

                if (val > 1)
                {
                    val = 1.0;
                    if (vi != end - 1)
                        doDraw = false;
                }

                if (val < -1)
                {
                    val = -1.0;
                    if (vi != start)
                        doDraw = false;
                }

                if (uni && val < 0)
                {
                    val = 0;
                    if (vi != start)
                        doDraw = false;
                }

                if (doDraw)
                {
                    // Little secret: unipolar is just a relabeled bipolar
                    float pval = val;

                    if (uni)
                    {
                        pval = val * 2 - 1;
                    }

                    vTicks.push_back(
                        std::make_tuple(pval, val, vi == 0 || vi == start || vi == end - 1));
                }
            }
        }
    }

    virtual void paint(juce::Graphics &g) override
    {
        auto uni = lfodata->unipolar.val.b;
        auto vs = getLocalBounds();

        if (ms->axisWidth < 0 || ms->axisStart < 0)
            zoomToFull();

        if (hotzones.empty())
            recalcHotZones(juce::Point<int>(vs.getX(), vs.getY()));

        g.fillAll(skin->getColor(Colors::MSEGEditor::Background));
        auto drawArea = getDrawArea();
        float maxt = drawDuration();
        auto valpx = valToPx();
        auto tpx = timeToPx();
        auto pxt = pxToTime();

        // Now draw the loop region
        if (ms->loopMode != MSEGStorage::LoopMode::ONESHOT && ms->editMode != MSEGStorage::LFO)
        {
            int ls = (ms->loop_start >= 0 ? ms->loop_start : 0);
            int le = (ms->loop_end >= 0 ? ms->loop_end : ms->n_activeSegments - 1);

            float pxs = tpx(ms->segmentStart[ls]);
            float pxe = tpx(ms->segmentEnd[le]);

            if (!(pxs > drawArea.getRight() || pxe < drawArea.getX()))
            {
                auto oldpxs = pxs, oldpxe = pxe;
                pxs = std::max(drawArea.getX(), (int)pxs);
                pxe = std::min(drawArea.getRight(), (int)pxe);

                auto loopRect =
                    juce::Rectangle<float>(juce::Point<float>(pxs - 0.5, drawArea.getY()),
                                           juce::Point<float>(pxe, drawArea.getBottom()));
                auto cf = skin->getColor(Colors::MSEGEditor::Loop::RegionFill);

                g.setColour(cf);
                g.fillRect(loopRect);

                auto cb = skin->getColor(Colors::MSEGEditor::Loop::RegionBorder);
                g.setColour(cb);

                if (oldpxe > drawArea.getX() && oldpxe <= drawArea.getRight())
                {
                    g.drawLine(pxe - 0.5, drawArea.getY(), pxe - 0.5, drawArea.getBottom(), 1);
                }
                if (oldpxs >= drawArea.getX() && oldpxs < drawArea.getRight())
                {
                    g.drawLine(pxs - 0.5, drawArea.getY(), pxs - 0.5, drawArea.getBottom(), 1);
                }
            }
        }

        Surge::MSEG::EvaluatorState es, esdf;
        // This is different from the number in LFOMS::assign in draw mode on purpose
        es.seed(8675309);
        esdf.seed(8675309);

        auto path = juce::Path();
        auto highlightPath = juce::Path();
        auto defpath = juce::Path();
        auto fillpath = juce::Path();

        float pathScale = 1.0;

        auto xdisp = drawArea;
        float yOff = drawArea.getY();

        auto beginP = [yOff, pathScale](juce::Path &p, float x, float y) {
            p.startNewSubPath(pathScale * x, pathScale * (y - yOff));
        };

        auto addP = [yOff, pathScale](juce::Path &p, float x, float y) {
            p.lineTo(pathScale * x, pathScale * (y - yOff));
        };

        bool hlpathUsed = false;

        float pathFirstY, pathLastX, pathLastY, pathLastDef;

        // this slightly odd construct means we always draw beyond the last point
        bool drawnLast = false;
        int priorEval = 0;

        for (int q = 0; q <= drawArea.getWidth(); ++q)
        {
            float up = pxt(q + drawArea.getX());
            int i = q;
            if (!drawnLast)
            {
                float iup = (int)up;
                float fup = up - iup;
                float v = Surge::MSEG::valueAt(iup, fup, 0, ms, &es, true);
                float vdef = Surge::MSEG::valueAt(iup, fup, lfodata->deform.val.f, ms, &esdf, true);

                v = valpx(v);
                vdef = valpx(vdef);
                // Brownian doesn't deform and the second display is confusing since it is
                // independently random
                if (es.lastEval >= 0 && es.lastEval <= ms->n_activeSegments - 1 &&
                    ms->segments[es.lastEval].type == MSEGStorage::segment::Type::BROWNIAN)
                    vdef = v;

                int compareWith = es.lastEval;

                if (up >= ms->totalDuration)
                    compareWith = ms->n_activeSegments - 1;

                if (compareWith != priorEval)
                {
                    // OK so make sure that priorEval nv1 is in there
                    addP(path, i, valpx(ms->segments[priorEval].nv1));
                    for (int ns = priorEval + 1; ns <= compareWith; ns++)
                    {
                        // Special case - hold draws endpoint
                        if (ns > 0 && ms->segments[ns - 1].type == MSEGStorage::segment::HOLD)
                            addP(path, i, valpx(ms->segments[ns - 1].v0));
                        addP(path, i, valpx(ms->segments[ns].v0));
                    }

                    if (priorEval == hoveredSegment && hlpathUsed)
                    {
                        addP(highlightPath, i, valpx(ms->segments[priorEval].nv1));
                    }

                    priorEval = es.lastEval;
                }

                if (es.lastEval == hoveredSegment)
                {
                    bool skipThisAdd = false;

                    // edge case when you go exactly up to 1 evenly. See #3940
                    if (up < ms->segmentStart[es.lastEval] || up > ms->segmentEnd[es.lastEval])
                        skipThisAdd = true;

                    if (!hlpathUsed)
                    {
                        // We always want to hit the start
                        if (ms->segmentStart[hoveredSegment] >= ms->axisStart)
                            beginP(highlightPath, i, valpx(ms->segments[hoveredSegment].v0));
                        else
                        {
                            skipThisAdd = true;
                            beginP(highlightPath, i, v);
                            beginP(highlightPath, i, v);
                        }

                        hlpathUsed = true;
                    }

                    if (!skipThisAdd)
                    {
                        addP(highlightPath, i, v);
                    }
                }

                if (i == 0)
                {
                    beginP(path, i, v);
                    beginP(defpath, i, vdef);
                    beginP(fillpath, i, v);
                    pathFirstY = v;
                }
                else
                {
                    addP(path, i, v);
                    addP(defpath, i, vdef);
                    addP(fillpath, i, v);
                }

                pathLastX = i;
                pathLastY = v;
                pathLastDef = vdef;
            }

            drawnLast = up > ms->totalDuration;
        }

        int uniLimit = uni ? -1 : 0;

        addP(fillpath, pathLastX, valpx(uniLimit));
        addP(fillpath, uniLimit, valpx(uniLimit));
        addP(fillpath, uniLimit, pathFirstY);

        auto tfpath = juce::AffineTransform()
                          .scaled(1.0 / pathScale, 1.0 / pathScale)
                          .translated(drawArea.getX(), drawArea.getY());

        juce::ColourGradient cg;

        if (uni)
        {
            cg = juce::ColourGradient::vertical(
                skin->getColor(Colors::MSEGEditor::GradientFill::StartColor),
                skin->getColor(Colors::MSEGEditor::GradientFill::EndColor), drawArea);
        }
        else
        {
            cg = juce::ColourGradient::vertical(
                skin->getColor(Colors::MSEGEditor::GradientFill::StartColor),
                skin->getColor(Colors::MSEGEditor::GradientFill::StartColor), drawArea);
            cg.addColour(0.5, skin->getColor(Colors::MSEGEditor::GradientFill::EndColor));
        }

        {
            juce::Graphics::ScopedSaveState gs(g);
            g.setGradientFill(cg);
            g.fillPath(fillpath, tfpath);
        }

        // draw vertical grid
        auto primaryGridColor = skin->getColor(Colors::MSEGEditor::Grid::Primary);
        auto secondaryHGridColor = skin->getColor(Colors::MSEGEditor::Grid::SecondaryHorizontal);
        auto secondaryVGridColor = skin->getColor(Colors::MSEGEditor::Grid::SecondaryVertical);

        updateHTicks();

        for (auto hp : hTicks)
        {
            auto t = hp.first;
            auto c = hp.second;
            auto px = tpx(t);
            auto linewidth = 1.f;
            int ticklen = 4;

            if (c & TickDrawStyle::kHighlight)
            {
                g.setColour(primaryGridColor);
            }
            else
            {
                g.setColour(secondaryVGridColor);
            }

            float pxa = px - linewidth * 0.5;

            if (pxa < drawArea.getX() || pxa > drawArea.getRight())
                continue;

            g.drawLine(pxa, drawArea.getY(), pxa, drawArea.getBottom() + ticklen, linewidth);
        }

        updateVTicks();

        for (auto vp : vTicks)
        {
            float val = std::get<0>(vp);
            float v = valpx(val);
            int ticklen = 0;
            auto sp = juce::PathStrokeType(1.0);

            if (std::get<2>(vp))
            {
                g.setColour(primaryGridColor);

                if (val != 0.f)
                {
                    ticklen = 20;
                }

                g.drawLine(drawArea.getX() - ticklen, v, drawArea.getRight(), v);
            }
            else
            {
                bool useDottedPath = false; // use juce::Path and dotted. Slow AF on windows
                bool useSolidPath = false;  // use juce::Path and solid. Fastest, but no dots
                bool useHand = !(useDottedPath || useSolidPath); // Hand roll the lines. Fast enough

                g.setColour(secondaryHGridColor);

                if (useHand)
                {
                    float x = drawArea.getX() - ticklen;
                    float xe = drawArea.getRight();

                    if (val != -1.f)
                    {
                        while (x < xe)
                        {
                            g.drawLine(x, v, std::min(xe, x + 2), v, 0.5);
                            x += 5;
                        }
                    }
                }
                else
                {
                    float dashLength[2] = {2.f, 5.f}; // 2 px dash 5 px gap
                    auto dotted = juce::Path();
                    auto markerLine = juce::Path();
                    auto st = juce::PathStrokeType(0.5, juce::PathStrokeType::beveled,
                                                   juce::PathStrokeType::butt);

                    markerLine.startNewSubPath(drawArea.getX() - ticklen, v);
                    markerLine.lineTo(drawArea.getRight(), v);

                    // Can be any even size if you change the '2' below
                    st.createDashedStroke(dotted, markerLine, dashLength, 2);

                    if (useDottedPath)
                    {
                        g.strokePath(dotted, st);
                    }
                    else
                    {
                        g.strokePath(markerLine, st);
                    }
                }
            }
        }

        // draw hover loop markers
        for (const auto &h : hotzones)
        {
            if (h.type == hotzone::LOOPMARKER)
            {
                /*
                 * OK the loop marker suppression is a wee bit complicated
                 * If the end is at 0, it draws over the axis; if the start is at end similar
                 * so handle the cases differently. Remember this is because the 'marker'
                 * of the start graphic is the left edge |< and the right end is >|
                 */
                if (h.zoneSubType == hotzone::LOOP_START)
                {
                    // my left edge is off the right or my left edge is off the left
                    if (h.rect.getX() > drawArea.getRight() + 1 ||
                        h.rect.getX() < drawArea.getX() - 1)
                    {
                        continue;
                    }
                }
                else if (h.zoneSubType == hotzone::LOOP_END)
                {
                    // my right edge is off the right or left
                    if (h.rect.getRight() > drawArea.getRight() + 1 ||
                        h.rect.getRight() <= drawArea.getX())
                    {
                        continue;
                    }
                }

                // draw a hovered loop marker
                if (h.active)
                {
                    auto c = skin->getColor(Colors::MSEGEditor::Loop::Marker);
                    auto hr = h.rect.translated(0, -1);

                    g.setColour(c);
                    g.fillRect(hr);

                    drawLoopDragMarker(g, c, h.zoneSubType, hr);
                }
            }
        }

        drawAxis(g);

        // draw segment curve
        auto strokeStyle = juce::PathStrokeType(0.75 * pathScale, juce::PathStrokeType::beveled,
                                                juce::PathStrokeType::butt);
        g.setColour(skin->getColor(Colors::MSEGEditor::DeformCurve));
        g.strokePath(defpath, strokeStyle, tfpath);

        strokeStyle = juce::PathStrokeType(1.0 * pathScale, juce::PathStrokeType::beveled,
                                           juce::PathStrokeType::butt);
        g.setColour(skin->getColor(Colors::MSEGEditor::Curve));
        g.strokePath(path, strokeStyle, tfpath);

        if (hlpathUsed)
        {
            strokeStyle = juce::PathStrokeType(1.5 * pathScale, juce::PathStrokeType::beveled,
                                               juce::PathStrokeType::butt);
            g.setColour(skin->getColor(Colors::MSEGEditor::CurveHighlight));
            g.strokePath(highlightPath, strokeStyle, tfpath);
        }

        for (const auto &h : hotzones)
        {
            if (h.type == hotzone::MOUSABLE_NODE)
            {
                int sz = 13;
                int offx = 0, offy = 0;
                bool showValue = false;
                const bool isMouseDown = (mouseDownInitiation == MOUSE_DOWN_IN_DRAW_AREA);

                if (h.active)
                {
                    offy = isMouseDown ? 1 : 0;
                    showValue = isMouseDown;
                }

                if (h.dragging)
                {
                    offy = 2;
                    showValue = isMouseDown;
                }

                if (h.zoneSubType == hotzone::SEGMENT_CONTROL)
                    offx = 1;

                if (lassoSelector && lassoSelector->contains(h.associatedSegment))
                    offy = 2;

                auto r = h.rect;

                if (h.useDrawRect)
                {
                    r = h.drawRect;
                }

                int cx = r.getCentreX();

                if (handleDrawable)
                {
                    if (cx >= drawArea.getX() && cx <= drawArea.getRight())
                    {
                        juce::Graphics::ScopedSaveState gs(g);
                        auto movet = juce::AffineTransform().translated(r.getTopLeft());

                        g.addTransform(movet);
                        g.reduceClipRegion(juce::Rectangle<int>(0, 0, sz, sz));

                        auto at = juce::AffineTransform().translated(-offx * sz, -offy * sz);

                        handleDrawable->draw(g, 1.0, at);
                    }
                }

                if (showValue)
                {
                    auto fillr = [&g](juce::Rectangle<float> r, juce::Colour c) {
                        g.setColour(c);
                        g.fillRect(r);
                    };

                    const int detailedMode = Surge::Storage::getValueDisplayPrecision(storage);

                    g.setFont(skin->fontManager->lfoTypeFont);

                    float val = h.specialEndpoint ? ms->segments[h.associatedSegment].nv1
                                                  : ms->segments[h.associatedSegment].v0;

                    std::string txt = fmt::format("X: {:.{}f}", pxt(cx), detailedMode),
                                txt2 = fmt::format("Y: {:.{}f}", val, detailedMode);

                    int sw1 = SST_STRING_WIDTH_INT(g.getCurrentFont(), txt),
                        sw2 = SST_STRING_WIDTH_INT(g.getCurrentFont(), txt2);

                    float dragX = r.getRight(), dragY = r.getBottom();
                    float dragW = 6 + std::max(sw1, sw2), dragH = 22;

                    // reposition the display if we've reached right or bottom edge of drawArea
                    if (dragX + dragW > drawArea.getRight())
                    {
                        dragX -= int(dragX + dragW) % drawArea.getRight();
                    }

                    if (dragY + dragH > drawArea.getBottom())
                    {
                        dragY -= int(dragY + dragH) % drawArea.getBottom();
                    }

                    auto readout = juce::Rectangle<float>(dragX, dragY, dragW, dragH);

                    // if the readout intersects with a node rect, shift it up
                    // (this only happens in bottom right corner)
                    if (readout.intersectRectangles(dragX, dragY, dragW, dragH, r.getX(), r.getY(),
                                                    r.getWidth(), r.getHeight()))
                    {
                        readout.translate(-(r.getHeight() / 2), -(r.getHeight() / 2));
                    }

                    fillr(readout, skin->getColor(Colors::LFO::StepSeq::InfoWindow::Border));
                    readout = readout.reduced(1, 1);
                    fillr(readout, skin->getColor(Colors::LFO::StepSeq::InfoWindow::Background));

                    readout = readout.withTrimmedLeft(2).withHeight(10);

                    g.setColour(skin->getColor(Colors::LFO::StepSeq::InfoWindow::Text));
                    g.drawText(txt, readout, juce::Justification::centredLeft);

                    readout = readout.translated(0, 10);

                    g.drawText(txt2, readout, juce::Justification::centredLeft);
                }
            }
        }
    }

    juce::Point<int> mouseDownOrigin, lastPanZoomMousePos, cursorHideOrigin;

    bool cursorResetPosition = false;
    bool inDrag = false;
    bool inDrawDrag = false;
    enum
    {
        NOT_MOUSE_DOWN,
        MOUSE_DOWN_IN_DRAW_AREA,
        MOUSE_DOWN_OUTSIDE_DRAW_AREA
    } mouseDownInitiation = NOT_MOUSE_DOWN;
    bool cursorHideEnqueued = false;

    void mouseDown(const juce::MouseEvent &e) override
    {
        prepareForUndo();
        mouseDownInitiation =
            (getDrawArea().contains(e.position.toInt()) ? MOUSE_DOWN_IN_DRAW_AREA
                                                        : MOUSE_DOWN_OUTSIDE_DRAW_AREA);
        if (e.mods.isPopupMenu())
        {
            auto da = getDrawArea();
            if (da.contains(e.position.toInt()))
            {
                openPopup(e.position);
            }
            else
            {
                /*
                 * Edge case: Hotzones can span the axis and we want the entire node
                 * to be right mouse clickable
                 */
                for (auto h : hotzones)
                {
                    if (h.rect.contains(e.position))
                    {
                        openPopup(e.position);
                    }
                }
            }
            return;
        }

        for (auto &s : ms->segments)
        {
            s.dragv0 = s.v0;
            s.dragv1 = s.nv1;
            s.dragcpv = s.cpv;
            s.dragDuration = s.duration;
        }

        if (e.mods.isShiftDown())
        {
            lasso = std::make_unique<juce::LassoComponent<MSEGLassoItem>>();
            // FIX ME SKIN COLOR
            lasso->setColour(juce::LassoComponent<MSEGLassoItem>::lassoFillColourId,
                             juce::Colour(220, 220, 250).withAlpha(0.1f));
            lassoSelector = std::make_unique<MSEGLassoSelector>(this);
            addAndMakeVisible(*lasso);

            repaint();
            lasso->beginLasso(e, lassoSelector.get());
            return;
        }

        if (lassoSelector)
        {
            bool clearLasso = true;
            for (auto &h : hotzones)
            {
                if (h.rect.contains(e.position.toFloat()) && h.type == hotzone::MOUSABLE_NODE &&
                    (h.zoneSubType == hotzone::SEGMENT_ENDPOINT ||
                     h.zoneSubType == hotzone::SEGMENT_CONTROL) &&
                    lassoSelector->contains(h.associatedSegment))
                    clearLasso = false;
            }

            if (timeEditMode == DRAW)
                clearLasso = false;

            if (clearLasso)
            {
                lassoSelector.reset(nullptr);
            }
            repaint();
        }

        auto where = e.position;

        if (timeEditMode == DRAW)
        {
            // Allow us to initiate drags on control points in draw mode
            bool amOverControl = false;
            for (auto &h : hotzones)
            {
                if (h.rect.contains(where) && h.type == hotzone::MOUSABLE_NODE &&
                    h.zoneSubType == hotzone::SEGMENT_CONTROL)
                {
                    amOverControl = true;
                }
            }

            if (!amOverControl)
            {
                auto da = getDrawArea();
                /*
                 * Only initiate draws in the draw area allowing the axis to still pan and zoom
                 */
                if (da.contains(where.toInt()) &&
                    !(e.mods.isShiftDown() || e.mods.isMiddleButtonDown()))
                {
                    /*
                     * Activate temporary snap but in draw mode, do vSnap only
                     */
                    bool a = e.mods.isAltDown();
                    if (a)
                    {
                        snapGuard = std::make_shared<SnapGuard>(this);
                        if (a)
                            ms->vSnap = ms->vSnapDefault;
                    }
                    inDrawDrag = true;
                    return;
                }
            }
        }

        mouseDownOrigin = where.toInt();
        lastPanZoomMousePos = where.toInt();
        inDrag = true;

        bool gotHZ = false;

        for (auto &h : hotzones)
        {
            if (h.rect.contains(where) && h.type == hotzone::MOUSABLE_NODE)
            {
                gotHZ = true;
                hideCursor(where.toInt());

                h.active = true;
                h.dragging = true;

                repaint();

                /*
                 * Activate temporary snap. Note this is also handled similarly in
                 * onMouseMoved so if you change ctrl/alt here change it there too
                 */
                bool c = e.mods.isCommandDown();
                bool a = e.mods.isAltDown();

                if (c || a)
                {
                    snapGuard = std::make_shared<SnapGuard>(this);

                    if (c)
                    {
                        ms->hSnap = ms->hSnapDefault;
                    }
                    if (a)
                    {
                        ms->vSnap = ms->vSnapDefault;
                    }
                }
                break;
            }

            if (h.rect.contains(where) && h.type == hotzone::LOOPMARKER)
            {
                gotHZ = true;
                hideCursor(where.toInt());
                h.active = true;
                h.dragging = true;
                repaint();
            }
        }

        if (!gotHZ)
        {
            // Lin doesn't cursor hide so this hand is bad there
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        }

        return;
    }

    void mouseDoubleClick(const juce::MouseEvent &event) override
    {
        auto ha = getHAxisDoubleClickArea();
        auto where = event.position.toInt();

        if (ha.contains(where))
        {
            zoomToFull();
            return;
        }

        auto da = getDrawArea();

        if (da.contains(where))
        {
            auto tf = pxToTime();
            auto pv = pxToVal();
            auto tp = timeToPx();
            auto vp = valToPx();

            auto t = tf(where.x);
            auto v = pv(where.y);

            auto testIfLastNode = [tp, vp](juce::Point<int> where, MSEGStorage *ms) {
                auto idx = ms->n_activeSegments;
                auto area = juce::Rectangle<int>(where.x, where.y, 4, 4);
                auto last_node =
                    juce::Point<int>(tp(ms->segmentEnd[idx]), vp(ms->segments[idx].nv1));

                return area.contains(last_node);
            };

            // Check if I'm on a hotzone
            for (auto &h : hotzones)
            {
                if (h.rect.contains(where.toFloat()) && h.type == hotzone::MOUSABLE_NODE)
                {
                    switch (h.zoneSubType)
                    {
                    case hotzone::SEGMENT_ENDPOINT:
                    {
                        if (ms->editMode == MSEGStorage::LFO && testIfLastNode(where, ms))
                        {
                            return;
                        }
                        else
                        {
                            if (event.mods.isShiftDown() && h.associatedSegment >= 0)
                            {
                                Surge::MSEG::deleteSegment(ms,
                                                           ms->segmentStart[h.associatedSegment]);
                            }
                            else
                            {
                                Surge::MSEG::unsplitSegment(ms, t);
                            }

                            modelChanged();
                            repaint();

                            return;
                        }
                    }
                    case hotzone::SEGMENT_CONTROL:
                    {
                        // Reset the control point to half segment duration, middle value
                        Surge::MSEG::resetControlPoint(ms, t);

                        modelChanged();
                        repaint();

                        return;
                    }
                    case hotzone::LOOP_START:
                    case hotzone::LOOP_END:
                        // FIXME : Implement this
                        break;
                    }
                }
            }

            if (t < ms->totalDuration)
            {
                if (event.mods.isShiftDown())
                {
                    Surge::MSEG::deleteSegment(ms, t);
                }
                else
                {
                    Surge::MSEG::splitSegment(ms, t, v);
                }

                modelChanged();
                repaint();

                return;
            }
            else
            {
                Surge::MSEG::extendTo(ms, t, v);

                modelChanged();
                repaint();

                return;
            }
        }

        guaranteeCursorShown();
    }

    void mouseUp(const juce::MouseEvent &e) override
    {
        pushToUndoIfDirty();
        auto where = e.position.toInt();

        mouseDownInitiation = NOT_MOUSE_DOWN;

        setMouseCursor(juce::MouseCursor::NormalCursor);
        inDrag = false;
        inDrawDrag = false;

        if (lasso)
        {
            lasso->endLasso();
            removeChildComponent(lasso.get());
            lasso.reset(nullptr);

            if (lassoSelector && lassoSelector->items.getNumSelected() == 0)
            {
                lassoSelector.reset(nullptr);
            }

            return;
        }

        if (loopDragTime >= 0)
        {
            loopDragTime = -1;
            repaint();
        }

        for (auto &h : hotzones)
        {
            if (h.dragging)
            {
                if (h.type == hotzone::MOUSABLE_NODE)
                {
                    if (h.zoneSubType == hotzone::SEGMENT_ENDPOINT)
                    {
                        showCursorAt(h.rect.getCentre());
                    }

                    if (h.zoneSubType == hotzone::SEGMENT_CONTROL && !h.useDrawRect)
                    {
                        showCursorAt(h.rect.getCentre());
                    }
                }

                if (h.type == hotzone::LOOPMARKER)
                {
                    showCursorAt(h.rect.getCentre());
                }
            }

            h.active = false;
            h.dragging = false;

            repaint();
        }

        snapGuard = nullptr;
        guaranteeCursorShown();

        return;
    }

    void showCursorAt(const juce::Point<float> &w) { cursorHideOrigin = w.toInt(); }
    void showCursorAt(const juce::Point<int> &w) { cursorHideOrigin = w; }

    void guaranteeCursorShown()
    {
        if (!Surge::GUI::showCursor(storage) && cursorHidden)
        {
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(false);
            auto h = localPointToGlobal(cursorHideOrigin);
            juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(h.toFloat());
            cursorHidden = false;
        }
    }

    bool cursorHidden = false;

    void hideCursor(const juce::Point<int> &w)
    {
        cursorHideOrigin = w;
        if (!Surge::GUI::showCursor(storage))
        {
            cursorHidden = true;
            juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(true);
        }
    }

    void mouseExit(const juce::MouseEvent &e) override
    {
        hoveredSegment = -1;
        repaint();
        return;
    }

    void mouseMagnify(const juce::MouseEvent &event, float scaleFactor) override
    {
        auto pii = event.position.toInt();
        zoom(pii, -(1.f - scaleFactor) * 4);
        return;
    }

    void mouseMove(const juce::MouseEvent &event) override
    {
        auto tf = pxToTime();
        auto t = tf(event.position.getX());
        auto da = getDrawArea();
        auto where = event.position.toInt();

        if (da.contains(where))
        {
            auto ohs = hoveredSegment;
            if (t < 0)
            {
                hoveredSegment = 0;
            }
            else if (t >= ms->totalDuration)
            {
                hoveredSegment = std::max(0, ms->n_activeSegments - 1);
            }
            else
            {
                hoveredSegment = Surge::MSEG::timeToSegment(ms, t);
            }
            if (hoveredSegment != ohs)
                repaint();
        }
        else
        {
            if (hoveredSegment >= 0)
            {
                hoveredSegment = -1;
                repaint();
            }
        }

        bool reset = false;

        for (const auto &h : hotzones)
        {
            if (!h.rect.contains(event.position))
                continue;

            reset = true;
            if (h.zoneSubType == hotzone::SEGMENT_CONTROL)
            {
                auto nextCursor = juce::MouseCursor::NormalCursor;
                switch (h.segmentDirection)
                {
                case hotzone::VERTICAL_ONLY:
                    nextCursor = juce::MouseCursor::UpDownResizeCursor;
                    break;
                case hotzone::HORIZONTAL_ONLY:
                    nextCursor = juce::MouseCursor::LeftRightResizeCursor;
                    break;
                case hotzone::BOTH_DIRECTIONS:
                    nextCursor = juce::MouseCursor::UpDownLeftRightResizeCursor;
                    break;
                }

                setMouseCursor(nextCursor);
            }

            if (h.zoneSubType == hotzone::SEGMENT_ENDPOINT)
                setMouseCursor(juce::MouseCursor::UpDownLeftRightResizeCursor);
        }

        if (!reset)
            setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    void mouseDrag(const juce::MouseEvent &event) override
    {
        if (lasso)
        {
            lasso->dragLasso(event);

            /*
             * You may think 'hey you can repaint locally' and try something like
             * this
             * auto lassoR = juce::Rectangle<float>(event.mouseDownPosition, event.position);
             * auto repaintR = lassoR.toNearestIntEdges().expanded(20);
             * repaint(repaintR);
             *
             * but control points can be arbitrarily far away. We could just
             * repaint the boxes of all points selected and their control points but
             * that is a hard list to find. So instead, when the lasso changes, we just:
             */
            repaint();
            return;
        }

        if (inDrawDrag)
        {
            // Activate temporary snap in vdirection only
            bool a = event.mods.isAltDown();
            if (a)
            {
                bool wasSnapGuard = true;
                if (!snapGuard)
                {
                    wasSnapGuard = false;
                    snapGuard = std::make_shared<SnapGuard>(this);
                }

                if (a)
                    ms->vSnap = ms->vSnapDefault;
                else if (wasSnapGuard)
                    ms->vSnap = snapGuard->vSnapO;
            }
            else if (!a && snapGuard)
            {
                snapGuard = nullptr;
            }

            auto tf = pxToTime();
            auto t = tf(event.position.x);
            auto pv = pxToVal();
            auto v = limit_range(pv(event.position.y), -1.f, 1.f);

            if (ms->vSnap > 0)
            {
                v = limit_range(round(v / ms->vSnap) * ms->vSnap, -1.f, 1.f);
            }

            int seg = Surge::MSEG::timeToSegment(this->ms, t);

            auto doAdjust = [this](int seg) {
                if (lassoSelector && lassoSelector->items.getNumSelected() > 0)
                    return lassoSelector->contains(seg);
                else
                    return true;
            };

            if (seg >= 0 && seg < ms->n_activeSegments &&
                t <= ms->totalDuration + MSEGStorage::minimumDuration)
            {
                bool ch = false;
                // OK are we near the endpoint of that segment
                if (t - ms->segmentStart[seg] <
                    std::max(ms->totalDuration * 0.05, 0.1 * ms->segments[seg].duration))
                {
                    if (doAdjust(seg))
                    {
                        ms->segments[seg].v0 = v;
                        ch = true;
                    }
                }
                else if (ms->segmentEnd[seg] - t <
                         std::max(ms->totalDuration * 0.05, 0.1 * ms->segments[seg].duration))
                {
                    int nx = seg + 1;
                    if (ms->endpointMode == MSEGStorage::EndpointMode::FREE)
                    {
                        if (nx == ms->n_activeSegments)
                        {
                            if (doAdjust(seg))
                            {
                                ms->segments[seg].nv1 = v;
                                ch = true;
                            }
                        }
                        else
                        {
                            if (doAdjust(nx))
                            {
                                ms->segments[nx].v0 = v;
                                ch = true;
                            }
                        }
                    }
                    else
                    {
                        if (nx >= ms->n_activeSegments)
                            nx = 0;
                        if (doAdjust(nx))
                        {
                            ms->segments[nx].v0 = v;
                            ch = true;
                        }
                    }
                }
                if (ch)
                {
                    modelChanged(seg);
                }
            }

            repaint();

            return;
        }

        bool gotOne = false;
        int idx = 0, draggingIdx = -1;
        bool foundDrag = false;
        auto where = event.position.toInt();
        std::set<int> modifyIndices;
        auto draggingType = hotzone::SEGMENT_ENDPOINT;
        for (auto &h : hotzones)
            if (h.dragging)
                draggingType = h.zoneSubType;

        for (auto &h : hotzones)
        {
            if (h.dragging || (lassoSelector && lassoSelector->contains(h.associatedSegment) &&
                               h.type == hotzone::MOUSABLE_NODE && h.zoneSubType == draggingType))
            {
                modifyIndices.insert(idx);
                foundDrag = true;
            }
            if (h.dragging)
                draggingIdx = idx;

            idx++;
        }

        if (foundDrag)
        {
            gotOne = true;
            // FIXME replace with distance APIs
            float dragX = where.getX() - mouseDownOrigin.x;
            float dragY = where.getY() - mouseDownOrigin.y;
            if (event.mods.isShiftDown())
            {
                dragX *= 0.2;
                dragY *= 0.2;
            }
            bool sep = false;
            holdOffOnModelChanged = true;
            for (auto idx : modifyIndices)
            {
                hotzones[idx].onDrag(dragX, dragY, juce::Point<float>(where.getX(), where.getY()));
                if (hotzones[idx].dragging)
                {
                    hoveredSegment = hotzones[idx].associatedSegment;
                    sep = hotzones[idx].specialEndpoint;
                }
            }
            holdOffOnModelChanged = false;
            modelChanged(hoveredSegment, sep, false);

            mouseDownOrigin = where;
        }

        if (gotOne)
        {
            Surge::MSEG::rebuildCache(ms);
            recalcHotZones(where);
            if (draggingIdx >= 0)
                hotzones[draggingIdx].dragging = true;

            repaint();
        }
        else if (event.mods.isLeftButtonDown() || event.mods.isMiddleButtonDown())
        {
            float x = where.x - mouseDownOrigin.x;
            float y = where.y - mouseDownOrigin.y;
            float r = sqrt(x * x + y * y);
            if (r > 3)
            {
                if (fabs(x) > fabs(y))
                {
                    float dx = where.x - lastPanZoomMousePos.x;
                    float panScale = 1.0 / getDrawArea().getWidth();
                    pan(where.toFloat(), -dx * panScale);
                }
                else
                {
                    float dy = where.y - lastPanZoomMousePos.y;
                    float zoomScale = 2. / getDrawArea().getHeight();
                    zoom(where.toInt(), -dy * zoomScale);
                }

                lastPanZoomMousePos = where;
                mouseDownOrigin = where;
            }
        }

        /*
         * Activate temporary snap. Note this is also checked in onMouseDown
         * so if you change ctrl/alt here change it there too
         */
        bool c = event.mods.isCommandDown();
        bool a = event.mods.isAltDown();

        if ((c || a))
        {
            bool wasSnapGuard = true;

            if (!snapGuard)
            {
                wasSnapGuard = false;
                snapGuard = std::make_shared<SnapGuard>(this);
            }
            if (c)
                ms->hSnap = ms->hSnapDefault;
            else if (wasSnapGuard)
                ms->hSnap = snapGuard->hSnapO;

            if (a)
                ms->vSnap = ms->vSnapDefault;
            else if (wasSnapGuard)
                ms->vSnap = snapGuard->vSnapO;
        }
        else if (!(c || a) && snapGuard)
        {
            snapGuard = nullptr;
        }
    }

    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override
    {
        if (wheel.isInertial)
            return;
        auto where = event.position.toInt();
        if (wheel.deltaX == 0 && wheel.deltaY == 0)
            return;

        float wheelFac = 1.0;
#if WINDOWS || LINUX
        wheelFac = 1.0; // configure speed for other mice
#endif

        if (fabs(wheel.deltaX) > fabs(wheel.deltaY))
        {
            pan(where.toFloat(), -wheelFac * wheel.deltaX);
        }
        else
        {
            zoom(where, (wheel.isReversed ? -1 : 1) * wheelFac * 1.3 * wheel.deltaY);
        }
    }

    void zoom(const juce::Point<int> &where, float amount)
    {
        if (fabs(amount) < 1e-4)
            return;
        float dWidth = amount * ms->axisWidth;
        auto pxt = pxToTime();
        float t = pxt(where.x);

        ms->axisWidth = ms->axisWidth - dWidth;

        /*
         * OK so we want to adjust pan so the mouse position is basically the same
         * - this is pxToTime
           auto drawArea = getDrawArea();
           float maxt = drawDuration();
           float tscale = 1.f * drawArea.getWidth() / maxt;

           // So px = t * tscale + drawarea;
           // So t = ( px - drawarea ) / tscale;
           return [tscale, drawArea, this ](float px) {
             return (px - drawArea.getX()) / tscale + ms->axisStart;
           };

         * so we want ms->axisStart to mean that (where.x - drawArea.getX() ) * drawDuration() /
         drawArea.width + as to be constant
         *
         * t = (WX-DAL) * DD / DAW + AS
         * AS = t - (WX_DAL) * DD / DAW
         *
         */
        auto DD = drawDuration();
        auto DA = getDrawArea();
        auto DAW = DA.getWidth();
        auto DAL = DA.getX();
        auto WX = where.x;
        ms->axisStart = std::max(t - (WX - DAL) * DD / DAW, 0.f);
        applyZoomPanConstraints();

        // BOOKMARK
        recalcHotZones(where);
        repaint();
        // std::cout << "ZOOM by " << amount <<  " AT " << where.x << " " << t << std::endl;
    }

    bool shouldDirty{true};
    struct SkipDirtyGuard
    {
        MSEGCanvas &canvas;
        bool osd;
        SkipDirtyGuard(MSEGCanvas &c) : canvas(c)
        {
            osd = canvas.shouldDirty;
            canvas.shouldDirty = false;
        }
        ~SkipDirtyGuard() { canvas.shouldDirty = osd; }
    };

    void zoomToFull()
    {
        auto g = SkipDirtyGuard(*this);
        zoomOutTo(ms->totalDuration);
    }

    void zoomOutTo(float duration)
    {
        auto g = SkipDirtyGuard(*this);
        ms->axisStart = 0.f;
        ms->axisWidth =
            (ms->editMode == MSEGStorage::EditMode::ENVELOPE) ? std::max(1.0f, duration) : 1.f;
        modelChanged(0, false);
    }

    void pan(const juce::Point<float> &where, float amount)
    {
        // std::cout << "PAN " << ms->axisStart << " " << ms->axisWidth << std::endl;
        ms->axisStart += ms->axisWidth * amount;
        ms->axisStart = std::max(ms->axisStart, 0.f);
        applyZoomPanConstraints();

        recalcHotZones(where.toInt());
        repaint();
    }

    // see if it can be done in a cleaner way
    // TODO: once we have more than 6 voice/scene LFOs, this condition will need tweaking,
    // because new LFOs will need to be appended at the end of the list of modsources
    // OR, we break compatibility in a major version update and have all LFOs in a block
    bool isSceneMSEG() const { return lfodata->shape.ctrlgroup_entry >= ms_slfo1; }

    void showSegmentTypein(int i, SegmentProps prop, const juce::Point<int> at)
    {
        using type = MSEGStorage::segment::Type;

        const bool is2Dcp =
            ms->segments[i].type == type::QUAD_BEZIER || ms->segments[i].type == type::BROWNIAN;

        float *propValue = nullptr;
        std::string propName;

        switch (prop)
        {
        case SegmentProps::duration:
            propValue = &ms->segments[i].duration;
            propName = "Duration";
            break;
        case SegmentProps::value:
            propValue = &ms->segments[i].v0;
            propName = "Value";
            break;
        case SegmentProps::cp_duration:
            propValue = &ms->segments[i].cpduration;
            propName = "Control Point X";
            break;
        case SegmentProps::cp_value:
            propValue = &ms->segments[i].cpv;
            propName = fmt::format("Control Point{}", is2Dcp ? " Y" : "");
            break;

        default:
            return;
        }

        const int detailedMode = Surge::Storage::getValueDisplayPrecision(storage);

        auto handleTypein = [this, i, prop, propValue, propName](const std::string &s) {
            auto divPos = s.find('/');
            float v = 0.f;

            if (divPos != std::string::npos)
            {
                auto n = s.substr(0, divPos);
                auto d = s.substr(divPos + 1);
                auto nv = std::atof(n.c_str());
                auto dv = std::atof(d.c_str());

                if (dv == 0)
                {
                    return false;
                }

                v = (nv / dv);
            }
            else
            {
                v = std::atof(s.c_str());
            }

            const float lowClamp =
                (prop == SegmentProps::duration || prop == SegmentProps::cp_duration) ? 0.f : -1.f;
            const float highClamp = (prop == SegmentProps::duration) ? 1000.f : 1.f;

            *propValue = std::clamp(v, lowClamp, highClamp);

            pushToUndo();
            modelChanged();
            repaint();

            return true;
        };

        if (!typeinEditor)
        {
            typeinEditor = std::make_unique<Surge::Overlays::TypeinLambdaEditor>(handleTypein);
            getParentComponent()->addChildComponent(*typeinEditor);
        }

        typeinEditor->callback = handleTypein;
        typeinEditor->setMainLabel(
            fmt::format("Edit Segment {} {}", std::to_string(i + 1), propName));
        typeinEditor->setValueLabels(fmt::format("current: {:.{}f}", *propValue, detailedMode), "");
        typeinEditor->setSkin(skin, associatedBitmapStore);
        typeinEditor->setEditableText(fmt::format("{:.{}f}", *propValue, detailedMode));
        typeinEditor->setReturnFocusTarget(this);

        auto r = typeinEditor->getRequiredSize();
        typeinEditor->setBounds(r.withCentre(at));

        typeinEditor->setVisible(true);
        typeinEditor->grabFocus();
    }

    void openPopup(const juce::Point<float> &iw)
    {
        using type = MSEGStorage::segment::Type;

        auto contextMenu = juce::PopupMenu();
        auto tf = pxToTime();
        auto t = tf(iw.x);
        auto tts = Surge::MSEG::timeToSegment(ms, t);

        if (hoveredSegment >= 0 && tts != hoveredSegment)
        {
            tts = hoveredSegment;
            t = ms->segmentStart[tts];
        }

        auto msurl =
            storage ? SurgeGUIEditor::helpURLForSpecial(storage, "mseg-editor") : std::string();
        auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);

        auto tcomp = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>("MSEG Segment", hurl);
        tcomp->setSkin(skin, associatedBitmapStore);
        auto hment = tcomp->getTitle();

        contextMenu.addCustomItem(-1, std::move(tcomp), nullptr, hment);

        contextMenu.addSeparator();

        const int detailedMode = Surge::Storage::getValueDisplayPrecision(storage);

        contextMenu.addItem(
            fmt::format("Duration: {:.{}f}", ms->segments[tts].duration, detailedMode), true, false,
            [this, iw, tts]() { showSegmentTypein(tts, SegmentProps::duration, iw.toInt()); });
        contextMenu.addItem(
            fmt::format("Value: {:.{}f}", ms->segments[tts].v0, detailedMode), true, false,
            [this, iw, tts]() { showSegmentTypein(tts, SegmentProps::value, iw.toInt()); });

        const bool is2Dcp =
            ms->segments[tts].type == type::QUAD_BEZIER || ms->segments[tts].type == type::BROWNIAN;

        if (ms->segments[tts].type != type::HOLD)
        {
            if (is2Dcp)
            {
                auto curveStr = fmt::format("Control Point X: {:.{}f}",
                                            ms->segments[tts].cpduration, detailedMode);

                contextMenu.addItem(curveStr, true, false, [this, iw, tts]() {
                    showSegmentTypein(tts, SegmentProps::cp_duration, iw.toInt());
                });
            }

            auto curveStr = fmt::format("Control Point{}: {:.{}f}", is2Dcp ? " Y" : "",
                                        ms->segments[tts].cpv, detailedMode);

            contextMenu.addItem(curveStr, true, false, [this, iw, tts]() {
                showSegmentTypein(tts, SegmentProps::cp_value, iw.toInt());
            });
        }

        contextMenu.addSeparator();

        if (ms->editMode != MSEGStorage::LFO && ms->loopMode != MSEGStorage::LoopMode::ONESHOT)
        {
            if (tts <= ms->loop_end + 1 && tts != ms->loop_start)
            {
                contextMenu.addItem(Surge::GUI::toOSCase("Set Loop Start"), [this, tts]() {
                    Surge::MSEG::setLoopStart(ms, tts);
                    pushToUndo();
                    modelChanged();
                });
            }

            if (tts >= ms->loop_start - 1 && tts != ms->loop_end)
            {
                contextMenu.addItem(Surge::GUI::toOSCase("Set Loop End"), [this, tts, t]() {
                    auto along = t - ms->segmentStart[tts];

                    if (ms->segments[tts].duration == 0)
                    {
                        along = 0;
                    }
                    else
                    {
                        along = along / ms->segments[tts].duration;
                    }

                    int target = tts;

                    if (along < 0.1 && tts > 0)
                    {
                        target = tts - 1;
                    }

                    Surge::MSEG::setLoopEnd(ms, target);
                    pushToUndo();
                    modelChanged();
                });
            }

            contextMenu.addSeparator();
        }

        if (tts >= 0)
        {
            auto actionsMenu = juce::PopupMenu();

            auto pv = pxToVal();
            auto v = pv(iw.y);

            bool isActive = (ms->editMode == MSEGStorage::ENVELOPE);

            actionsMenu.addItem("Split", [this, t, v]() {
                Surge::MSEG::splitSegment(this->ms, t, v);
                pushToUndo();
                modelChanged();
            });

            actionsMenu.addItem("Delete", (ms->n_activeSegments > 1), false, [this, t]() {
                Surge::MSEG::deleteSegment(this->ms, t);
                pushToUndo();
                modelChanged();
            });

            actionsMenu.addSeparator();

            actionsMenu.addItem(Surge::GUI::toOSCase("Double Duration"), isActive, false, [this]() {
                Surge::MSEG::scaleDurations(this->ms, 2.0, longestMSEG);
                pushToUndo();
                modelChanged();
                zoomToFull();
            });

            actionsMenu.addItem(Surge::GUI::toOSCase("Half Duration"), isActive, false, [this]() {
                Surge::MSEG::scaleDurations(this->ms, 0.5, longestMSEG);
                pushToUndo();
                modelChanged();
                zoomToFull();
            });

            actionsMenu.addSeparator();

            actionsMenu.addItem(Surge::GUI::toOSCase("Flip Vertically"), [this]() {
                Surge::MSEG::scaleValues(this->ms, -1);
                pushToUndo();
                modelChanged();
            });

            actionsMenu.addItem(Surge::GUI::toOSCase("Flip Horizontally"), [this]() {
                Surge::MSEG::mirrorMSEG(this->ms);
                pushToUndo();
                modelChanged();
            });

            actionsMenu.addSeparator();

            actionsMenu.addItem(Surge::GUI::toOSCase("Quantize Nodes to Snap Divisions"), isActive,
                                false, [this]() {
                                    Surge::MSEG::setAllDurationsTo(this->ms, ms->hSnapDefault);
                                    pushToUndo();
                                    modelChanged();
                                });

            actionsMenu.addItem(Surge::GUI::toOSCase("Quantize Nodes to Whole Units"), isActive,
                                false, [this]() {
                                    Surge::MSEG::setAllDurationsTo(this->ms, 1.0);
                                    pushToUndo();
                                    modelChanged();
                                });

            actionsMenu.addItem(Surge::GUI::toOSCase("Distribute Nodes Evenly"), [this]() {
                Surge::MSEG::setAllDurationsTo(this->ms,
                                               ms->totalDuration / this->ms->n_activeSegments);
                pushToUndo();
                modelChanged();
            });

            contextMenu.addSubMenu("Actions", actionsMenu);

            auto createMenu = juce::PopupMenu();

            createMenu.addItem(Surge::GUI::toOSCase("Minimal MSEG"), [this]() {
                Surge::MSEG::clearMSEG(this->ms);
                this->zoomToFull();
                if (controlregion)
                    controlregion->rebuild();
                pushToUndo();
                modelChanged();
            });

            createMenu.addSeparator();

            createMenu.addItem(Surge::GUI::toOSCase("Default Voice MSEG"), [this]() {
                Surge::MSEG::createInitVoiceMSEG(this->ms);
                this->zoomToFull();
                if (controlregion)
                    controlregion->rebuild();
                pushToUndo();
                modelChanged();
            });

            createMenu.addItem(Surge::GUI::toOSCase("Default Scene MSEG"), [this]() {
                Surge::MSEG::createInitSceneMSEG(this->ms);
                this->zoomToFull();
                if (controlregion)
                    controlregion->rebuild();
                pushToUndo();
                modelChanged();
            });

            createMenu.addSeparator();

            int stepCounts[] = {8, 16, 32, 64, 128};

            for (int i : stepCounts)
            {
                createMenu.addItem(Surge::GUI::toOSCase(std::to_string(i) + " Step Sequencer"),
                                   [this, i]() {
                                       Surge::MSEG::createStepseqMSEG(this->ms, i);
                                       this->zoomToFull();
                                       if (controlregion)
                                           controlregion->rebuild();
                                       pushToUndo();
                                       modelChanged();
                                   });
            }

            createMenu.addSeparator();

            for (int i : stepCounts)
            {
                createMenu.addItem(Surge::GUI::toOSCase(std::to_string(i) + " Lines Sine"),
                                   [this, i] {
                                       Surge::MSEG::createSinLineMSEG(this->ms, i);
                                       this->zoomToFull();
                                       if (controlregion)
                                           controlregion->rebuild();
                                       pushToUndo();
                                       modelChanged();
                                   });
            }

            createMenu.addSeparator();

            for (int i : stepCounts)
            {
                // we're fine with a max of 32 sawtooth plucks methinks - ED
                if (i <= 32)
                {
                    createMenu.addItem(Surge::GUI::toOSCase(std::to_string(i) + " Sawtooth Plucks"),
                                       [this, i]() {
                                           Surge::MSEG::createSawMSEG(this->ms, i, 0.5);
                                           this->zoomToFull();
                                           if (controlregion)
                                               controlregion->rebuild();
                                           pushToUndo();
                                           modelChanged();
                                       });
                }
            }

            contextMenu.addSubMenu("Create", createMenu);

            if (!isSceneMSEG())
            {
                auto triggerMenu = juce::PopupMenu();

                triggerMenu.addItem(Surge::GUI::toOSCase("Filter EG"), true,
                                    (ms->segments[tts].retriggerFEG), [this, tts]() {
                                        this->ms->segments[tts].retriggerFEG =
                                            !this->ms->segments[tts].retriggerFEG;
                                        pushToUndo();
                                        modelChanged();
                                    });

                triggerMenu.addItem(Surge::GUI::toOSCase("Amp EG"), true,
                                    (ms->segments[tts].retriggerAEG), [this, tts]() {
                                        this->ms->segments[tts].retriggerAEG =
                                            !this->ms->segments[tts].retriggerAEG;
                                        pushToUndo();
                                        modelChanged();
                                    });

                triggerMenu.addSeparator();

                triggerMenu.addItem(Surge::GUI::toOSCase("Nothing"), [this, tts]() {
                    this->ms->segments[tts].retriggerFEG = false;
                    this->ms->segments[tts].retriggerAEG = false;
                    pushToUndo();
                    modelChanged();
                });

                triggerMenu.addItem(Surge::GUI::toOSCase("All"), [this, tts]() {
                    this->ms->segments[tts].retriggerFEG = true;
                    this->ms->segments[tts].retriggerAEG = true;
                    pushToUndo();
                    modelChanged();
                });

                contextMenu.addSubMenu("Trigger", triggerMenu);
            }

            auto settingsMenu = juce::PopupMenu();

            settingsMenu.addItem(
                Surge::GUI::toOSCase("Link Start and End Nodes"), true,
                (ms->endpointMode == MSEGStorage::EndpointMode::LOCKED), [this]() {
                    pushToUndo();
                    if (this->ms->endpointMode == MSEGStorage::EndpointMode::LOCKED)
                    {
                        this->ms->endpointMode = MSEGStorage::EndpointMode::FREE;
                    }
                    else
                    {
                        this->ms->endpointMode = MSEGStorage::EndpointMode::LOCKED;
                        this->ms->segments[ms->n_activeSegments - 1].nv1 = this->ms->segments[0].v0;
                        modelChanged();
                    }
                });

            settingsMenu.addSeparator();

            settingsMenu.addItem(Surge::GUI::toOSCase("Deform Applied to Segment"), true,
                                 ms->segments[tts].useDeform, [this, tts]() {
                                     this->ms->segments[tts].useDeform =
                                         !this->ms->segments[tts].useDeform;
                                     pushToUndo();
                                     modelChanged();
                                 });

            settingsMenu.addItem(Surge::GUI::toOSCase("Invert Deform Value"), true,
                                 ms->segments[tts].invertDeform, [this, tts]() {
                                     this->ms->segments[tts].invertDeform =
                                         !this->ms->segments[tts].invertDeform;
                                     pushToUndo();
                                     modelChanged();
                                 });

            contextMenu.addSubMenu("Settings", settingsMenu);

            contextMenu.addSeparator();

            auto typeTo = [this, &contextMenu, t, tts](std::string n,
                                                       MSEGStorage::segment::Type type) {
                contextMenu.addItem(
                    n, true, (tts >= 0 && this->ms->segments[tts].type == type), [this, t, type]() {
                        // ADJUST HERE
                        Surge::MSEG::changeTypeAt(this->ms, t, type);

                        for (auto &h : hotzones)
                        {
                            if (lassoSelector && lassoSelector->contains(h.associatedSegment) &&
                                h.type == hotzone::MOUSABLE_NODE &&
                                h.zoneSubType == hotzone::SEGMENT_ENDPOINT)
                            {
                                this->ms->segments[h.associatedSegment].type = type;
                            }
                        }

                        pushToUndo();
                        modelChanged();
                    });
            };

            typeTo("Hold", type::HOLD);
            typeTo("Linear", type::LINEAR);
            typeTo(Surge::GUI::toOSCase("S-Curve"), type::SCURVE);
            typeTo("Bezier", type::QUAD_BEZIER);

            contextMenu.addSeparator();

            typeTo("Sine", type::SINE);
            typeTo("Triangle", type::TRIANGLE);
            typeTo("Sawtooth", type::SAWTOOTH);
            typeTo("Square", type::SQUARE);

            contextMenu.addSeparator();

            typeTo("Bump", type::BUMP);
            typeTo("Stairs", type::STAIRS);
            typeTo(Surge::GUI::toOSCase("Smooth Stairs"), type::SMOOTH_STAIRS);
            typeTo(Surge::GUI::toOSCase("Brownian Bridge"), type::BROWNIAN);

            contextMenu.showMenuAsync(sge->popupMenuOptions());
        }
    }

    bool holdOffOnModelChanged{false};

    void modelChanged(int activeSegment = -1, bool specialEndpoint = false, bool rchz = true)
    {
        if (holdOffOnModelChanged)
            return;

        Surge::MSEG::rebuildCache(ms);
        applyZoomPanConstraints(activeSegment, specialEndpoint);
        if (rchz)
            recalcHotZones(mouseDownOrigin); // FIXME

        if (shouldDirty)
        {
            storage->getPatch().isDirty = true;
            tagForUndo();
        }
        this->sge->forceLfoDisplayRepaint();
        onModelChanged();
        repaint();
    }

    static constexpr int longestMSEG = 128;
    static constexpr int longestSmallZoom = 32;

    void applyZoomPanConstraints(int activeSegment = -1, bool specialEndpoint = false)
    {
        if (ms->editMode == MSEGStorage::LFO)
        {
            // Reset axis bounds
            if (ms->axisWidth > 1)
                ms->axisWidth = 1;
            if (ms->axisStart + ms->axisWidth > 1)
                ms->axisStart = 1.0 - ms->axisWidth;
            if (ms->axisStart < 0)
                ms->axisStart = 0;
        }
        else
        {
            auto bd = std::max(ms->totalDuration, 1.f);
            auto longest = bd * 2;
            if (longest > longestMSEG)
                longest = longestMSEG;
            if (longest < longestSmallZoom)
                longest = longestSmallZoom;
            if (ms->axisWidth > longest)
            {
                ms->axisWidth = longest;
            }
            else if (ms->axisStart + ms->axisWidth > longest)
            {
                ms->axisStart = longest - ms->axisWidth;
            }

            // This is how we pan as we stretch
            if (activeSegment >= 0)
            {
                if (specialEndpoint)
                {
                    if (ms->segmentEnd[activeSegment] >= ms->axisStart + ms->axisWidth)
                    {
                        // In this case we are dragging the endpoint
                        ms->axisStart = ms->segmentEnd[activeSegment] - ms->axisWidth;
                    }
                    else if (ms->segmentEnd[activeSegment] <= ms->axisStart)
                    {
                        ms->axisStart = ms->segmentEnd[activeSegment];
                    }
                }
                else if (ms->segmentStart[activeSegment] >= ms->axisStart + ms->axisWidth)
                {
                    ms->axisStart = ms->segmentStart[activeSegment] - ms->axisWidth;
                }
                else if (ms->segmentStart[activeSegment] <= ms->axisStart)
                {
                    ms->axisStart = ms->segmentStart[activeSegment];
                }
            }
        }
        ms->axisWidth = std::max(ms->axisWidth, 0.05f);
    }

    using MSEGLassoItem = int;

    struct MSEGLassoSelector : public juce::LassoSource<MSEGLassoItem>
    {

        MSEGLassoSelector(MSEGCanvas *c) : canvas(c), items() {}

        void findLassoItemsInArea(juce::Array<MSEGLassoItem> &itemsFound,
                                  const juce::Rectangle<int> &area) override
        {
            for (auto h : canvas->hotzones)
            {
                if (h.type == hotzone::MOUSABLE_NODE && h.zoneSubType == hotzone::SEGMENT_ENDPOINT)
                {
                    if (h.rect.toNearestInt().intersects(area))
                        itemsFound.add(h.associatedSegment);
                }
            }
        }

        juce::SelectedItemSet<MSEGLassoItem> &getLassoSelection() override { return items; }

        bool contains(int msegIndex) const
        {
            auto res = items.isSelected(msegIndex);
            return res;
        }

        juce::SelectedItemSet<MSEGLassoItem> items;
        MSEGCanvas *canvas;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MSEGLassoSelector);
    };

    std::unique_ptr<juce::LassoComponent<MSEGLassoItem>> lasso;
    std::unique_ptr<MSEGLassoSelector> lassoSelector;

    int hoveredSegment = -1;
    MSEGStorage *ms;
    MSEGEditor::State *eds;
    LFOStorage *lfodata;
    MSEGControlRegion *controlregion = nullptr;
    SurgeStorage *storage = nullptr;
    float loopDragTime = -1, loopDragEnd = -1;
    bool loopDragIsStart = false;
    SurgeGUIEditor *sge;

    SurgeImage *handleDrawable{nullptr};

    bool shouldPostUndo{false};
    MSEGStorage pushThisToUndoStorage;
    void prepareForUndo()
    {
        pushThisToUndoStorage = *(this->ms);
        shouldPostUndo = false;
    }
    void tagForUndo() { shouldPostUndo = true; }
    void pushToUndo()
    {
        shouldPostUndo = false;
        sge->undoManager()->pushMSEG(sge->getCurrentScene(), sge->getCurrentLFOIDInEditor(),
                                     pushThisToUndoStorage);
    }
    void pushToUndoIfDirty()
    {
        if (shouldPostUndo)
        {
            pushToUndo();
        }
    }
};

void MSEGControlRegion::valueChanged(Surge::GUI::IComponentTagValue *p)
{
    auto tag = p->getTag();
    auto val = p->getValue();

    switch (tag)
    {
    case tag_edit_mode:
    {
        canvas->prepareForUndo();
        canvas->pushToUndo();
        int m = val > 0.5 ? 1 : 0;
        auto editMode = (MSEGStorage::EditMode)m;
        Surge::MSEG::modifyEditMode(this->ms, editMode);

        // zoom to fit
        canvas->ms->axisStart = 0.f;
        canvas->ms->axisWidth = editMode ? 1.f : ms->envelopeModeDuration;

        canvas->modelChanged(0, false);
        canvas->repaint();

        repaint();
        break;
    }
    case tag_loop_mode:
    {
        canvas->prepareForUndo();
        canvas->pushToUndo();
        int m = floor((val * 2) + 0.1) + 1;
        ms->loopMode = (MSEGStorage::LoopMode)m;
        canvas->modelChanged();
        canvas->repaint();
        repaint();
        break;
    }
    case tag_segment_movement_mode:
    {
        int m = floor((val * 2) + 0.5);

        eds->timeEditMode = m;

        if (canvas)
        {
            canvas->timeEditMode = (MSEGCanvas::TimeEdit)m;
            canvas->recalcHotZones(juce::Point<int>(0, 0));
            canvas->repaint();
        }
        repaint();
        break;
    }
    case tag_horizontal_snap:
    {
        ms->hSnap = (val < 0.5) ? 0.f : ms->hSnap = ms->hSnapDefault;
        if (canvas)
            canvas->repaint();

        break;
    }
    case tag_vertical_snap:
    {
        ms->vSnap = (val < 0.5) ? 0.f : ms->vSnap = ms->vSnapDefault;
        if (canvas)
            canvas->repaint();

        break;
    }
    case tag_vertical_value:
    {
        auto fv = 1.f / std::max(1, vSnapSize->getIntValue());
        ms->vSnapDefault = fv;
        if (ms->vSnap > 0)
            ms->vSnap = ms->vSnapDefault;
        if (canvas)
            canvas->repaint();
        break;
    }
    case tag_horizontal_value:
    {
        auto fv = 1.f / std::max(1, hSnapSize->getIntValue());
        ms->hSnapDefault = fv;
        if (ms->hSnap > 0)
            ms->hSnap = ms->hSnapDefault;
        if (canvas)
            canvas->repaint();
        break;
    }
    default:
        break;
    }
}

int32_t MSEGControlRegion::controlModifierClicked(Surge::GUI::IComponentTagValue *pControl,
                                                  const juce::ModifierKeys &button,
                                                  bool isDoubleClick)
{
    int tag = pControl->getTag();

    // Basically all the menus are a list of options with values
    std::vector<std::pair<std::string, float>> options;

    bool isOnOff = false;
    bool hasTypein = false;
    std::string menuName = "";

    switch (tag)
    {
    case tag_segment_movement_mode:
        menuName = "MSEG Movement Mode";
        options.push_back(std::make_pair("Single", 0));
        options.push_back(std::make_pair("Shift", 0.5));
        options.push_back(std::make_pair("Draw", 1.0));
        break;

    case tag_loop_mode:
        menuName = "MSEG Loop Mode";
        options.push_back(std::make_pair("Off", 0));
        options.push_back(std::make_pair("Loop", 0.5));
        options.push_back(
            std::make_pair(Surge::GUI::toOSCase("Gate (Loop Until Release)").c_str(), 1.0));
        break;

    case tag_edit_mode:
        menuName = "MSEG Edit Mode";
        options.push_back(std::make_pair("Envelope", 0));
        options.push_back(std::make_pair("LFO", 1.0));
        break;

    case tag_vertical_snap:
        menuName = "MSEG Vertical Snap";
    case tag_horizontal_snap:
        if (menuName == "")
        {
            menuName = "MSEG Horizontal Snap";
        }

        isOnOff = true;
        break;

    case tag_vertical_value:
        menuName = "MSEG Vertical Snap Grid";
    case tag_horizontal_value:
    {
        hasTypein = true;

        if (menuName == "")
        {
            menuName = "MSEG Horizontal Snap Grid";
        }

        auto addStop = [&options](int v) {
            options.push_back(
                std::make_pair(std::to_string(v), Parameter::intScaledToFloat(v, 100, 1)));
        };

        addStop(1);
        addStop(2);
        addStop(3);
        addStop(4);
        addStop(5);
        addStop(6);
        addStop(7);
        addStop(8);
        addStop(9);
        addStop(10);
        addStop(12);
        addStop(16);
        addStop(24);
        addStop(32);

        break;
    }
    default:
        break;
    }

    if (!options.empty() || isOnOff)
    {
        auto contextMenu = juce::PopupMenu();

        auto msurl = SurgeGUIEditor::helpURLForSpecial(storage, "mseg-editor");
        auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);
        auto tcomp = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(menuName, hurl);

        tcomp->setSkin(skin, associatedBitmapStore);
        auto hment = tcomp->getTitle();

        contextMenu.addCustomItem(-1, std::move(tcomp), nullptr, hment);

        contextMenu.addSeparator();

        if (!isOnOff)
        {
            for (auto op : options)
            {
                auto val = op.second;

                contextMenu.addItem(op.first, true, (val == pControl->getValue()),
                                    [val, pControl, this]() {
                                        pControl->setValue(val);
                                        valueChanged(pControl);

                                        auto iv = pControl->asJuceComponent();

                                        if (iv)
                                        {
                                            iv->repaint();
                                        }
                                    });
            }
        }

        if (hasTypein)
        {
            contextMenu.addSeparator();

            auto c = pControl->asJuceComponent();

            auto handleTypein = [c, pControl, this](const std::string &s) {
                auto i = std::atoi(s.c_str());

                if (i >= 1 && i <= 100)
                {
                    pControl->setValue(Parameter::intScaledToFloat(i, 100, 1));
                    valueChanged(pControl);

                    if (c)
                    {
                        c->repaint();
                    }

                    return true;
                }
                return false;
            };

            auto val =
                std::to_string(Parameter::intUnscaledFromFloat(pControl->getValue(), 100, 1));

            auto showTypein = [this, c, handleTypein, menuName, pControl, val]() {
                if (!typeinEditor)
                {
                    typeinEditor =
                        std::make_unique<Surge::Overlays::TypeinLambdaEditor>(handleTypein);
                    getParentComponent()->addChildComponent(*typeinEditor);
                }

                typeinEditor->callback = handleTypein;
                typeinEditor->setMainLabel(menuName);
                typeinEditor->setValueLabels("current: " + val, "");
                typeinEditor->setSkin(skin, associatedBitmapStore);
                typeinEditor->setEditableText(val);
                typeinEditor->setReturnFocusTarget(c);

                auto topOfControl = c->getParentComponent()->getY();
                auto pb = c->getBounds();
                auto cx = pb.getCentreX();

                auto r = typeinEditor->getRequiredSize();
                cx -= r.getWidth() / 2;
                r = r.withBottomY(topOfControl).withX(cx);
                typeinEditor->setBounds(r);

                typeinEditor->setVisible(true);
                typeinEditor->grabFocus();
            };

            contextMenu.addItem(Surge::GUI::toOSCase("Edit Value: ") + val, true, false,
                                showTypein);
        }

        contextMenu.showMenuAsync(sge->popupMenuOptions(),
                                  Surge::GUI::makeEndHoverCallback(pControl));
    }
    return 1;
}

void MSEGControlRegion::hvSnapTypein(bool isH)
{
    // So gross that this is copied from the menu handler partly above, but different enough that I
    // can't quite make it one thing. Ahh well. Will all get redone one day anyway
    if (!typeinEditor)
    {
        typeinEditor = std::make_unique<TypeinLambdaEditor>([](auto c) { return false; });
        getParentComponent()->addChildComponent(*typeinEditor);
    }
    typeinEditor->setReturnFocusTarget(isH ? hSnapSize.get() : vSnapSize.get());
    typeinEditor->setSkin(skin, associatedBitmapStore);

    typeinEditor->setMainLabel(std::string() + "MSEG " + (isH ? "Horizontal" : "Vertical") +
                               " Snap Grid");
    typeinEditor->setValueLabels(
        "current: " + std::to_string(isH ? hSnapSize->getIntValue() : vSnapSize->getIntValue()),
        "");
    typeinEditor->setEditableText(
        std::to_string(isH ? hSnapSize->getIntValue() : vSnapSize->getIntValue()));

    auto c = isH ? hSnapSize.get() : vSnapSize.get();
    auto topOfControl = c->getParentComponent()->getY();
    auto pb = c->getBounds();
    auto cx = pb.getCentreX();

    auto r = typeinEditor->getRequiredSize();
    cx -= r.getWidth() / 2;
    r = r.withBottomY(topOfControl).withX(cx);
    typeinEditor->setBounds(r);

    typeinEditor->setVisible(true);
    typeinEditor->grabKeyboardFocus();

    typeinEditor->callback = [w = juce::Component::SafePointer(this), isH](auto s) {
        auto sn = std::clamp(std::atoi(s.c_str()), 1, 32);
        if (w)
        {
            w->typeinEditor->setVisible(false);
            if (isH)
                w->hSnapSize->setIntValue(sn);
            else
                w->vSnapSize->setIntValue(sn);
        }
        return true;
    };
}

void MSEGControlRegion::rebuild()
{
    removeAllChildren();
    labels.clear();

    auto labelFont = skin->fontManager->getLatoAtSize(9, juce::Font::bold);
    auto editFont = skin->fontManager->getLatoAtSize(9);

    int height = getHeight();
    int margin = 2;
    int labelHeight = 12;
    int buttonHeight = 14;
    int numfieldHeight = 12;
    int xpos = 10;

    // movement modes
    {
        int segWidth = 110;
        int marginPos = xpos + margin;
        int btnWidth = 94;
        int ypos = 1;

        // label
        auto mml = std::make_unique<juce::Label>();
        mml->setText("Movement Mode", juce::dontSendNotification);
        mml->setFont(labelFont);
        mml->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
        mml->setBounds(marginPos, ypos, btnWidth, labelHeight);
        addAndMakeVisible(*mml);
        labels.push_back(std::move(mml));

        ypos += margin + labelHeight;

        // button
        auto btnrect = juce::Rectangle<int>(marginPos, ypos - 1, btnWidth, buttonHeight);

        movementMode = std::make_unique<Surge::Widgets::MultiSwitch>();
        movementMode->setBounds(btnrect);
        movementMode->setStorage(storage);
        movementMode->addListener(this);
        movementMode->setTag(tag_segment_movement_mode);
        movementMode->setHeightOfOneImage(buttonHeight);
        movementMode->setRows(1);
        movementMode->setColumns(3);
        movementMode->setDraggable(true);
        movementMode->setSkin(skin, associatedBitmapStore);

        auto dbl =
            skin->standardHoverAndHoverOnForIDB(IDB_MSEG_MOVEMENT_MODE, associatedBitmapStore);
        movementMode->setSwitchDrawable(std::get<0>(dbl));
        movementMode->setHoverSwitchDrawable(std::get<1>(dbl));
        movementMode->setHoverOnSwitchDrawable(std::get<2>(dbl));

        // canvas is nullptr in rebuild call in constructor, use state in this case
        if (canvas)
        {
            movementMode->setValue(float(canvas->timeEditMode) / 2.f);
        }
        else
        {
            movementMode->setValue(eds->timeEditMode / 2.f);
        }
        addAndMakeVisible(*movementMode);
        // this value centers the loop mode and snap sections against the MSEG editor width
        // if more controls are to be added to that center section, reduce this value
        xpos += 173;
    }

    // edit mode
    {
        int segWidth = 107;
        int btnWidth = 90;
        int ypos = 1;

        auto mml = std::make_unique<juce::Label>();
        mml->setText("Edit Mode", juce::dontSendNotification);
        mml->setFont(labelFont);
        mml->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
        mml->setBounds(xpos, ypos, segWidth, labelHeight);
        addAndMakeVisible(*mml);
        labels.push_back(std::move(mml));

        ypos += margin + labelHeight;

        // button
        auto btnrect = juce::Rectangle<int>(xpos, ypos - 1, btnWidth, buttonHeight);

        editMode = std::make_unique<Surge::Widgets::MultiSwitch>();
        editMode->setBounds(btnrect);
        editMode->setStorage(storage);
        editMode->addListener(this);
        editMode->setTag(tag_edit_mode);
        editMode->setHeightOfOneImage(buttonHeight);
        editMode->setRows(1);
        editMode->setColumns(2);
        editMode->setDraggable(true);
        editMode->setSkin(skin, associatedBitmapStore);

        auto dbl = skin->standardHoverAndHoverOnForIDB(IDB_MSEG_EDIT_MODE, associatedBitmapStore);
        editMode->setSwitchDrawable(std::get<0>(dbl));
        editMode->setHoverSwitchDrawable(std::get<1>(dbl));
        editMode->setHoverOnSwitchDrawable(std::get<2>(dbl));

        editMode->setValue(ms->editMode);
        addAndMakeVisible(*editMode);

        xpos += segWidth;
    }

    // loop mode
    {
        int segWidth = 110;
        int btnWidth = 94;
        int ypos = 1;

        auto mml = std::make_unique<juce::Label>();
        mml->setText("Loop Mode", juce::dontSendNotification);
        mml->setFont(labelFont);
        mml->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
        mml->setBounds(xpos, ypos, segWidth, labelHeight);
        addAndMakeVisible(*mml);
        labels.push_back(std::move(mml));

        ypos += margin + labelHeight;

        // button
        auto btnrect = juce::Rectangle<int>(xpos, ypos - 1, btnWidth, buttonHeight);
        loopMode = std::make_unique<Surge::Widgets::MultiSwitch>();
        loopMode->setBounds(btnrect);
        loopMode->setStorage(storage);
        loopMode->addListener(this);
        loopMode->setTag(tag_loop_mode);
        loopMode->setHeightOfOneImage(buttonHeight);
        loopMode->setRows(1);
        loopMode->setColumns(3);
        loopMode->setDraggable(true);
        loopMode->setSkin(skin, associatedBitmapStore);

        auto dbl = skin->standardHoverAndHoverOnForIDB(IDB_MSEG_LOOP_MODE, associatedBitmapStore);
        loopMode->setSwitchDrawable(std::get<0>(dbl));
        loopMode->setHoverSwitchDrawable(std::get<1>(dbl));
        loopMode->setHoverOnSwitchDrawable(std::get<2>(dbl));

        loopMode->setValue((ms->loopMode - 1) / 2.f);
        addAndMakeVisible(*loopMode);

        xpos += segWidth;
    }

    // snap to grid
    {
        int btnWidth = 49, editWidth = 32;
        int margin = 2;
        int segWidth = btnWidth + editWidth + 10;
        int ypos = 1;
        std::string svt;

        auto mml = std::make_unique<juce::Label>();
        mml->setText("Snap to Grid", juce::dontSendNotification);
        mml->setFont(labelFont);
        mml->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
        mml->setBounds(xpos, ypos, segWidth * 2 - 5, labelHeight);
        addAndMakeVisible(*mml);
        labels.push_back(std::move(mml));

        ypos += margin + labelHeight;

        hSnapButton = std::make_unique<Surge::Widgets::Switch>();
        hSnapButton->setSkin(skin, associatedBitmapStore);
        hSnapButton->setStorage(storage);
        hSnapButton->addListener(this);
        hSnapButton->setBounds(xpos, ypos - 1, btnWidth, buttonHeight);
        hSnapButton->setTag(tag_horizontal_snap);
        hSnapButton->setValue(ms->hSnap < 0.001 ? 0 : 1);
        auto hsbmp = associatedBitmapStore->getImage(IDB_MSEG_HORIZONTAL_SNAP);
        hSnapButton->setSwitchDrawable(hsbmp);
        auto hoverBmp = skin->hoverBitmapOverlayForBackgroundBitmap(
            nullptr, dynamic_cast<SurgeImage *>(hsbmp), associatedBitmapStore,
            Surge::GUI::Skin::HoverType::HOVER);
        hSnapButton->setHoverSwitchDrawable(hoverBmp);
        addAndMakeVisible(*hSnapButton);

        svt = fmt::format("{:d}", (int)round(1.f / ms->hSnapDefault));

        hSnapSize = std::make_unique<Surge::Widgets::NumberField>();
        hSnapSize->setControlMode(Surge::Skin::Parameters::MSEG_SNAP_H);
        hSnapSize->addListener(this);
        hSnapSize->setTag(tag_horizontal_value);
        hSnapSize->setStorage(storage);
        hSnapSize->setSkin(skin, associatedBitmapStore);
        hSnapSize->setIntValue(round(1.f / ms->hSnapDefault));
        hSnapSize->setBounds(xpos + 52 + margin, ypos, editWidth, numfieldHeight);
        auto images =
            skin->standardHoverAndHoverOnForIDB(IDB_MSEG_SNAPVALUE_NUMFIELD, associatedBitmapStore);
        hSnapSize->setBackgroundDrawable(images[0]);
        hSnapSize->setHoverBackgroundDrawable(images[1]);
        hSnapSize->setTextColour(skin->getColor(Colors::MSEGEditor::NumberField::Text));
        hSnapSize->setHoverTextColour(skin->getColor(Colors::MSEGEditor::NumberField::TextHover));
        hSnapSize->onReturnPressed = [w = juce::Component::SafePointer(this)](auto tag, auto nf) {
            if (w)
            {
                w->hvSnapTypein(true);
                return true;
            }
            return false;
        };
        addAndMakeVisible(*hSnapSize);

        xpos += segWidth;

        vSnapButton = std::make_unique<Surge::Widgets::Switch>();
        vSnapButton->setSkin(skin, associatedBitmapStore);
        vSnapButton->setStorage(storage);
        vSnapButton->addListener(this);
        vSnapButton->setBounds(xpos, ypos - 1, btnWidth, buttonHeight);
        vSnapButton->setTag(tag_vertical_snap);
        vSnapButton->setValue(ms->vSnap < 0.001 ? 0 : 1);
        auto vsbmp = associatedBitmapStore->getImage(IDB_MSEG_VERTICAL_SNAP);
        vSnapButton->setSwitchDrawable(vsbmp);
        hoverBmp = skin->hoverBitmapOverlayForBackgroundBitmap(
            nullptr, vsbmp, associatedBitmapStore, Surge::GUI::Skin::HoverType::HOVER);
        vSnapButton->setHoverSwitchDrawable(hoverBmp);
        addAndMakeVisible(*vSnapButton);

        svt = fmt::format("{:d}", (int)round(1.f / ms->vSnapDefault));

        vSnapSize = std::make_unique<Surge::Widgets::NumberField>();
        vSnapSize->setTag(tag_vertical_value);
        vSnapSize->setControlMode(Surge::Skin::Parameters::MSEG_SNAP_V);
        vSnapSize->addListener(this);
        vSnapSize->setStorage(storage);
        vSnapSize->setSkin(skin, associatedBitmapStore);
        vSnapSize->setIntValue(round(1.f / ms->vSnapDefault));
        vSnapSize->setBounds(xpos + 52 + margin, ypos, editWidth, numfieldHeight);
        images =
            skin->standardHoverAndHoverOnForIDB(IDB_MSEG_SNAPVALUE_NUMFIELD, associatedBitmapStore);
        vSnapSize->setBackgroundDrawable(images[0]);
        vSnapSize->setHoverBackgroundDrawable(images[1]);
        vSnapSize->setTextColour(skin->getColor(Colors::MSEGEditor::NumberField::Text));
        vSnapSize->setHoverTextColour(skin->getColor(Colors::MSEGEditor::NumberField::TextHover));
        vSnapSize->onReturnPressed = [w = juce::Component::SafePointer(this)](auto tag, auto nf) {
            if (w)
            {
                w->hvSnapTypein(false);
                return true;
            }
            return false;
        };
        addAndMakeVisible(*vSnapSize);
    }
}

MSEGEditor::MSEGEditor(SurgeStorage *storage, LFOStorage *lfodata, MSEGStorage *ms, State *eds,
                       Surge::GUI::Skin::ptr_t skin, std::shared_ptr<SurgeImageStore> bmp,
                       SurgeGUIEditor *sge)
    : OverlayComponent("MSEG Editor")
{
    // Leave these in for now
    if (ms->n_activeSegments <= 0) // This is an error state! Compensate
    {
        Surge::MSEG::createInitVoiceMSEG(ms);
    }
    setSkin(skin, bmp);

    canvas = std::make_unique<MSEGCanvas>(storage, lfodata, ms, eds, skin, bmp, sge);
    controls =
        std::make_unique<MSEGControlRegion>(nullptr, storage, lfodata, ms, eds, skin, bmp, sge);

    canvas->controlregion = controls.get();
    controls->canvas = canvas.get();

    canvas->onModelChanged = [this]() { this->onModelChanged(); };

    addAndMakeVisible(*controls);
    addAndMakeVisible(*canvas);
}

MSEGEditor::~MSEGEditor() = default;

void MSEGEditor::forceRefresh()
{
    for (auto *kid : getChildren())
    {
        auto ed = dynamic_cast<MSEGCanvas *>(kid);
        if (ed)
            ed->modelChanged();
        auto cr = dynamic_cast<MSEGControlRegion *>(kid);
        if (cr)
            cr->rebuild();
    }
}

void MSEGEditor::paint(juce::Graphics &g) { g.fillAll(juce::Colours::black); }

void MSEGEditor::resized()
{
    int controlHeight = 35;

    auto t = getTransform().inverted();
    auto h = getHeight();
    auto w = getWidth();
    t.transformPoint(w, h);
    auto r = getLocalBounds().withWidth(w).withHeight(h);

    auto cvr = r.withTrimmedBottom(controlHeight);
    auto ctr = r.withTop(cvr.getBottom());

    canvas->setBounds(cvr);
    canvas->recalcHotZones(juce::Point<int>(0, 0));
    controls->setBounds(ctr);
    // mained setbounds
}

} // namespace Overlays
} // namespace Surge

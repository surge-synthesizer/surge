"""
Tests for Surge XT Python bindings.
"""

import contextlib
import faulthandler
import glob
import os

import numpy as np
import pytest
import surgepy


def test_getVersion():
    version = surgepy.getVersion()
    assert isinstance(version, str)


def test_createSurge():
    surgepy.createSurge(44100)


def test_render_note():
    """
    Test rendering a note into a buffer.
    """
    s = surgepy.createSurge(44100)
    n_blocks = int(2 * s.getSampleRate() / s.getBlockSize())
    buf = s.createMultiBlock(n_blocks)
    s.playNote(0, 60, 127, 0)
    s.processMultiBlock(buf)
    assert not np.all(buf == 0.0)

def test_render_note_with_input():
    """
    Test rendering a note with an input buffer into an output buffer.
    """
    s = surgepy.createSurge(44100)
    n_blocks = int(2 * s.getSampleRate() / s.getBlockSize())
    in_buf = s.createMultiBlock(n_blocks)
    in_buf[0] = np.random.normal(0, 1000, size=in_buf[0].size)
    in_buf[1] = np.random.normal(0, 1000, size=in_buf[1].size)
    out_buf = s.createMultiBlock(n_blocks)
    s.playNote(0, 60, 127, 0)
    s.processMultiBlockWithInput(in_buf, out_buf)
    assert not np.all(out_buf == 0.0)


def test_default_mpeEnabled():
    """
    Test that mpeEnabled flag is False by default.
    """
    s = surgepy.createSurge(44100)
    assert s.mpeEnabled is False


def test_set_mpeEnabled():
    """
    Test that setting mpeEnabled really changes its value.
    """
    s = surgepy.createSurge(44100)
    s.mpeEnabled = True
    assert s.mpeEnabled is True


def test_default_tuningApplicationMode():
    s = surgepy.createSurge(44100)
    assert s.tuningApplicationMode == surgepy.TuningApplicationMode.RETUNE_MIDI_ONLY


def test_set_tuningApplicationMode():
    s = surgepy.createSurge(44100)
    s.tuningApplicationMode = surgepy.TuningApplicationMode.RETUNE_ALL
    assert s.tuningApplicationMode == surgepy.TuningApplicationMode.RETUNE_ALL


def test_param_extend_range():
    """
    Test reading and writing the extended range of a parameter.
    """
    s = surgepy.createSurge(44100)
    pitch = s.getPatch()["scene"][0]["osc"][0]["pitch"]

    assert s.canExtend(pitch) is True
    assert s.getExtend(pitch) is False

    s.setExtend(pitch, True)
    assert s.getExtend(pitch) is True

    # Extending osc pitch turns semitones into semitones times twelve
    s.setParamVal(pitch, 1)
    assert s.getParamDisplay(pitch).startswith("12.00")


def test_param_tempo_sync():
    """
    Test reading and writing the tempo sync of a parameter.
    """
    s = surgepy.createSurge(44100)
    rate = s.getPatch()["scene"][0]["lfo"][0]["rate"]

    assert s.canTempoSync(rate) is True
    assert s.getTempoSync(rate) is False

    s.setTempoSync(rate, True)
    assert s.getTempoSync(rate) is True
    assert "note" in s.getParamDisplay(rate)


def test_param_absolute():
    """
    Test reading and writing the absolute mode of a parameter.
    """
    s = surgepy.createSurge(44100)
    pitch = s.getPatch()["scene"][0]["osc"][0]["pitch"]

    assert s.canBeAbsolute(pitch) is True
    assert s.getAbsolute(pitch) is False

    s.setAbsolute(pitch, True)
    assert s.getAbsolute(pitch) is True


def test_param_deform():
    """
    Test reading and writing the deform type of a parameter.
    """
    s = surgepy.createSurge(44100)
    deform = s.getPatch()["scene"][0]["lfo"][0]["deform"]

    assert s.canDeform(deform) is True
    assert s.getDeform(deform) == 0

    s.setDeform(deform, 2)
    assert s.getDeform(deform) == 2


def test_param_portamento_options():
    """
    Test reading and writing the portamento options of a parameter.
    """
    s = surgepy.createSurge(44100)
    porta = s.getPatch()["scene"][0]["portamento"]

    assert s.canPortamento(porta) is True
    assert s.getPortamentoOptions(porta) == {
        "constantRate": False,
        "glissando": False,
        "retrigger": False,
        "curve": surgepy.constants.porta_lin,
    }

    # Options which aren't given are left alone
    s.setPortamentoOptions(porta, glissando=True, curve=surgepy.constants.porta_exp)
    assert s.getPortamentoOptions(porta) == {
        "constantRate": False,
        "glissando": True,
        "retrigger": False,
        "curve": surgepy.constants.porta_exp,
    }

    with pytest.raises(ValueError):
        s.setPortamentoOptions(porta, curve=7)


def test_param_deactivate():
    """
    Test reading and writing the deactivated state of a parameter.
    """
    s = surgepy.createSurge(44100)
    rate = s.getPatch()["scene"][0]["lfo"][0]["rate"]

    assert s.canDeactivate(rate) is True
    assert s.getDeactivated(rate) is False

    s.setDeactivated(rate, True)
    assert s.getDeactivated(rate) is True
    assert "Parameter deactivated: on" in s.getParamInfo(rate)


def test_param_features_on_unsupported_param():
    """
    Test that setting a feature a parameter doesn't have raises rather than being ignored.
    """
    s = surgepy.createSurge(44100)
    volume = s.getPatch()["volume"]

    assert s.canExtend(volume) is False
    assert s.canDeform(volume) is False
    assert s.canDeactivate(volume) is False
    assert s.canPortamento(volume) is False

    with pytest.raises(ValueError):
        s.setExtend(volume, True)

    with pytest.raises(ValueError):
        s.setDeform(volume, 1)

    with pytest.raises(ValueError):
        s.setDeactivated(volume, True)

    with pytest.raises(ValueError):
        s.getPortamentoOptions(volume)


def test_save_wavetable_round_trip(tmp_path):
    """
    Test that a factory wavetable loaded into an oscillator saves back out byte for byte.
    """
    s = surgepy.createSurge(44100)
    wavetables = sorted(
        glob.glob(os.path.join(s.getFactoryDataPath(), "wavetables", "**", "*.wt"), recursive=True)
    )

    if not wavetables:
        pytest.skip("No factory wavetables are installed")

    source = wavetables[0]
    osc = s.getPatch()["scene"][0]["osc"][0]
    s.setParamVal(osc["type"], surgepy.constants.ot_wavetable)
    s.loadWavetable(0, 0, source)

    target = str(tmp_path / "exported.wt")
    assert s.saveWavetable(0, 0, target) is True

    with open(source, "rb") as f, open(target, "rb") as g:
        assert f.read() == g.read()


def test_save_wavetable_without_a_wavetable(tmp_path):
    """
    Test that an oscillator which doesn't use wavetable data has nothing to save.
    """
    s = surgepy.createSurge(44100)

    with pytest.raises(ValueError):
        s.saveWavetable(0, 0, str(tmp_path / "nope.wt"))

    with pytest.raises(ValueError):
        s.saveWavetable(0, 9, str(tmp_path / "nope.wt"))


def test_mseg_is_a_live_reference():
    """
    Test that editing an MSEG object edits the patch, including after the expression which
    produced it has gone.
    """
    s = surgepy.createSurge(44100)
    s.getMSEG(0, 0).segments[0].duration = 2.5

    assert s.getMSEG(0, 0).segments[0].duration == 2.5
    assert s.getMSEG(0, 1).segments[0].duration != 2.5

    # The MSEG and its segments keep the synth they came from alive
    segments = surgepy.createSurge(44100).getMSEG(1, 3).segments
    segments[0].v0 = 0.5

    assert segments[0].v0 == 0.5


def test_mseg_segment_edits():
    """
    Test writing each of a segment's fields.
    """
    s = surgepy.createSurge(44100)
    seg = s.getMSEG(0, 0).segments[1]

    assert seg.index == 1

    seg.duration = 0.75
    seg.v0 = -0.25
    seg.cpduration = 0.25
    seg.cpv = 0.5
    seg.type = surgepy.constants.mseg_seg_quad_bezier
    seg.useDeform = False
    seg.invertDeform = True
    seg.retriggerFEG = True
    seg.retriggerAEG = True

    fresh = s.getMSEG(0, 0).segments[1]

    assert fresh.duration == 0.75
    assert fresh.v0 == -0.25
    assert fresh.cpduration == 0.25
    assert fresh.cpv == 0.5
    assert fresh.type == surgepy.constants.mseg_seg_quad_bezier
    assert fresh.useDeform is False
    assert fresh.invertDeform is True
    assert fresh.retriggerFEG is True
    assert fresh.retriggerAEG is True


def test_mseg_edits_rebuild_the_cache():
    """
    Test that a duration change updates the derived total duration, rather than leaving the MSEG
    playing back to stale segment times.
    """
    s = surgepy.createSurge(44100)
    mseg = s.getMSEG(0, 0)
    before = mseg.totalDuration

    mseg.segments[0].duration += 1.5

    assert mseg.totalDuration == pytest.approx(before + 1.5)


def test_mseg_insert_and_delete_segments():
    """
    Test growing and shrinking an MSEG.
    """
    s = surgepy.createSurge(44100)
    mseg = s.getMSEG(0, 0)
    count = mseg.segmentCount

    mseg.insertSegment(mseg.segmentCount)
    assert mseg.segmentCount == count + 1
    assert len(mseg.segments) == count + 1

    mseg.deleteSegment(0)
    assert mseg.segmentCount == count

    with pytest.raises(ValueError):
        mseg.insertSegment(mseg.segmentCount + 1)

    with pytest.raises(ValueError):
        mseg.deleteSegment(mseg.segmentCount)

    while mseg.segmentCount > 1:
        mseg.deleteSegment(0)

    # An MSEG with no segments is not something the engine can evaluate
    with pytest.raises(ValueError):
        mseg.deleteSegment(0)


def test_mseg_lfo_edit_mode_rescales_durations():
    """
    Test that switching to LFO edit mode constrains the MSEG to a single cycle.
    """
    s = surgepy.createSurge(44100)
    mseg = s.getMSEG(0, 0)

    assert mseg.editMode == surgepy.constants.mseg_editmode_env

    mseg.editMode = surgepy.constants.mseg_editmode_lfo

    assert mseg.totalDuration == pytest.approx(1.0)
    assert surgepy.validateMSEG(mseg) == []


def test_mseg_endpoint_value():
    """
    Test that the MSEG's end value is only writable while the endpoint is free.
    """
    s = surgepy.createSurge(44100)
    mseg = s.getMSEG(0, 0)

    mseg.endpointMode = surgepy.constants.mseg_endpoint_free
    mseg.endValue = -0.75
    assert mseg.endValue == -0.75

    # Locked endpoints follow the first segment, so there is nothing to set
    mseg.segments[0].v0 = 0.25
    mseg.endpointMode = surgepy.constants.mseg_endpoint_locked
    assert mseg.endValue == 0.25

    with pytest.raises(ValueError):
        mseg.endValue = -0.75


def test_mseg_loop_points():
    """
    Test the loop points, including the value which means the whole MSEG loops.
    """
    s = surgepy.createSurge(44100)
    mseg = s.getMSEG(0, 0)

    mseg.loopMode = surgepy.constants.mseg_loop_on
    mseg.loopStart = 1
    mseg.loopEnd = 2

    assert mseg.loopStart == 1
    assert mseg.loopEnd == 2
    assert surgepy.validateMSEG(mseg) == []

    mseg.loopStart = surgepy.constants.mseg_unset
    mseg.loopEnd = surgepy.constants.mseg_unset
    assert surgepy.validateMSEG(mseg) == []

    with pytest.raises(ValueError):
        mseg.loopStart = -1

    with pytest.raises(ValueError):
        mseg.loopEnd = mseg.segmentCount

    with pytest.raises(ValueError):
        mseg.loopMode = 17


@pytest.mark.parametrize(
    "field,value",
    [
        ("duration", -1.0),
        ("v0", 1.5),
        ("cpv", -1.5),
        ("cpduration", 2.0),
        ("type", 0),
        ("type", 999),
    ],
)
def test_mseg_segment_rejects_bad_values(field, value):
    """
    Test that a value the engine can't make sense of raises rather than landing in the patch.
    """
    s = surgepy.createSurge(44100)
    seg = s.getMSEG(0, 0).segments[0]

    with pytest.raises(ValueError):
        setattr(seg, field, value)


def test_mseg_segment_rejects_the_separator_type():
    """
    Test that the hole in the middle of the segment type enum, which the MSEG editor uses as a menu
    separator, isn't accepted as a segment type.
    """
    s = surgepy.createSurge(44100)
    seg = s.getMSEG(0, 0).segments[0]
    between = surgepy.constants.mseg_seg_sawtooth + 1

    assert between != surgepy.constants.mseg_seg_bump

    with pytest.raises(ValueError):
        seg.type = between


def test_validate_mseg_reports_lfo_duration_sum():
    """
    Test that validateMSEG catches durations which don't add up to a cycle in LFO edit mode, which
    the engine has no recovery for.
    """
    s = surgepy.createSurge(44100)
    mseg = s.getMSEG(0, 0)

    mseg.editMode = surgepy.constants.mseg_editmode_lfo
    assert surgepy.validateMSEG(mseg) == []

    mseg.segments[0].duration += 1.0
    problems = surgepy.validateMSEG(mseg)

    assert len(problems) == 1
    assert "LFO edit mode" in problems[0]


def test_set_mseg_copies_between_slots():
    """
    Test assigning one LFO's MSEG onto another's.
    """
    s = surgepy.createSurge(44100)
    source = s.getMSEG(0, 0)
    source.segments[0].duration = 2.5
    source.segments[1].type = surgepy.constants.mseg_seg_bump

    s.setMSEG(1, 3, source)
    target = s.getMSEG(1, 3)

    assert target.totalDuration == source.totalDuration
    assert target.segments[0].duration == 2.5
    assert target.segments[1].type == surgepy.constants.mseg_seg_bump

    # Assigning a slot onto itself leaves it alone rather than copying over itself
    s.setMSEG(1, 3, target)
    assert s.getMSEG(1, 3).segments[0].duration == 2.5


def test_set_mseg_rejects_an_invalid_mseg():
    """
    Test that an MSEG which doesn't validate can't be assigned.
    """
    s = surgepy.createSurge(44100)
    broken = s.getMSEG(0, 0)
    broken.editMode = surgepy.constants.mseg_editmode_lfo
    broken.segments[0].duration += 1.0

    with pytest.raises(ValueError):
        s.setMSEG(1, 3, broken)


def test_mseg_round_trips_through_a_patch(tmp_path):
    """
    Test that an MSEG edited from Python is saved with the patch and read back.
    """
    s = surgepy.createSurge(44100)
    s.setParamVal(s.getPatch()["scene"][0]["lfo"][0]["shape"], surgepy.constants.lt_mseg)

    mseg = s.getMSEG(0, 0)
    mseg.endpointMode = surgepy.constants.mseg_endpoint_free
    mseg.loopMode = surgepy.constants.mseg_loop_off
    mseg.segments[0].duration = 2.5
    mseg.segments[0].type = surgepy.constants.mseg_seg_bump
    mseg.endValue = -0.75

    target = str(tmp_path / "mseg.fxp")
    s.savePatch(target)

    t = surgepy.createSurge(44100)
    t.loadPatch(target)
    loaded = t.getMSEG(0, 0)

    assert loaded.loopMode == surgepy.constants.mseg_loop_off
    assert loaded.segments[0].duration == 2.5
    assert loaded.segments[0].type == surgepy.constants.mseg_seg_bump
    assert loaded.endValue == -0.75
    assert surgepy.validateMSEG(loaded) == []


def test_formula_round_trips_through_a_patch(tmp_path):
    """
    Test reading and writing a formula modulator's Lua body, and that it is saved with the patch.
    """
    s = surgepy.createSurge(44100)
    s.setParamVal(s.getPatch()["scene"][0]["lfo"][0]["shape"], surgepy.constants.lt_formula)

    assert "function process" in s.getFormula(0, 0)

    formula = (
        "function init(state) return state end\n"
        "function process(state)\n"
        "    state.output = state.phase * 2 - 1\n"
        "    return state\n"
        "end\n"
    )
    s.setFormula(0, 0, formula)

    assert s.getFormula(0, 0) == formula
    assert s.getFormula(0, 1) != formula

    target = str(tmp_path / "formula.fxp")
    s.savePatch(target)

    t = surgepy.createSurge(44100)
    t.loadPatch(target)

    assert t.getFormula(0, 0) == formula


@contextlib.contextmanager
def without_faulthandler():
    """
    LuaJIT reports a compile error by raising a Windows structured exception, which it catches
    itself, but which faulthandler prints a whole fatal-exception traceback for on the way past.
    """
    was_enabled = faulthandler.is_enabled()
    faulthandler.disable()

    try:
        yield
    finally:
        if was_enabled:
            faulthandler.enable()


def test_check_formula():
    """
    Test that a formula is stored whether or not it compiles, and that checkFormula says which.
    """
    s = surgepy.createSurge(44100)

    assert s.checkFormula(0, 0) == ""

    s.setFormula(0, 0, "this is not Lua")

    assert s.getFormula(0, 0) == "this is not Lua"

    with without_faulthandler():
        assert s.checkFormula(0, 0) != ""

    s.setFormula(0, 0, "function process(state) state.output = 0 return state end")

    assert s.checkFormula(0, 0) == ""


def test_mseg_and_formula_scene_lfo_range():
    """
    Test that an out of range scene or LFO raises on each of the accessors.
    """
    s = surgepy.createSurge(44100)

    for call in (s.getMSEG, s.getFormula, s.checkFormula):
        with pytest.raises(ValueError):
            call(2, 0)

        with pytest.raises(ValueError):
            call(0, 99)

    with pytest.raises(ValueError):
        s.setFormula(0, 99, "")

    with pytest.raises(ValueError):
        s.setMSEG(0, 99, s.getMSEG(0, 0))

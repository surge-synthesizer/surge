"""
Tests for Surge XT Python bindings.
"""

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


def test_param_features_on_unsupported_param():
    """
    Test that setting a feature a parameter doesn't have raises rather than being ignored.
    """
    s = surgepy.createSurge(44100)
    volume = s.getPatch()["volume"]

    assert s.canExtend(volume) is False
    assert s.canDeform(volume) is False
    assert s.canPortamento(volume) is False

    with pytest.raises(ValueError):
        s.setExtend(volume, True)

    with pytest.raises(ValueError):
        s.setDeform(volume, 1)

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

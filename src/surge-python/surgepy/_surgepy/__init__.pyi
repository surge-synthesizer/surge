"""
Python bindings for Surge XT Synthesizer
"""
from __future__ import annotations
import numpy
import typing
from . import constants
__all__ = ['SurgeControlGroup', 'SurgeControlGroupEntry', 'SurgeMSEG', 'SurgeMSEGSegment', 'SurgeModRouting', 'SurgeModSource', 'SurgeNamedParamId', 'SurgeSynthesizer', 'SurgeSynthesizer_ID', 'TuningApplicationMode', 'constants', 'createSurge', 'getVersion', 'validateMSEG']
class SurgeControlGroup:
    def __repr__(self) -> str:
        ...
    def getEntries(self) -> list[SurgePyControlGroupEntry]:
        ...
    def getId(self) -> int:
        ...
    def getName(self) -> str:
        ...
class SurgeControlGroupEntry:
    def __repr__(self) -> str:
        ...
    def getEntry(self) -> int:
        ...
    def getParams(self) -> list[SurgePyNamedParam]:
        ...
    def getScene(self) -> int:
        ...
class SurgeMSEG:
    def __repr__(self) -> str:
        ...
    def deleteSegment(self, index: int) -> None:
        """
        Delete the segment at an index.
        """
    def insertSegment(self, index: int) -> None:
        """
        Insert a segment at an index, or at segmentCount to append one.
        """
    def rebuildCache(self) -> None:
        """
        Recompute the derived segment times. Editing through this object does this already, so this is only needed after the patch has been changed some other way.
        """
    @property
    def editMode(self) -> int:
        """
        surgepy.constants.mseg_editmode_env for an MSEG of any length, or mseg_editmode_lfo to constrain it to a single cycle. Switching rescales the durations.
        """
    @editMode.setter
    def editMode(self, arg1: int) -> None:
        ...
    @property
    def endValue(self) -> float:
        """
        Value the MSEG finishes on. Only settable in free endpoint mode - in locked mode it follows the first segment's value.
        """
    @endValue.setter
    def endValue(self, arg1: float) -> None:
        ...
    @property
    def endpointMode(self) -> int:
        """
        surgepy.constants.mseg_endpoint_locked to make the MSEG end where it starts, or mseg_endpoint_free to give it its own endValue.
        """
    @endpointMode.setter
    def endpointMode(self, arg1: int) -> None:
        ...
    @property
    def hSnap(self) -> float:
        """
        Horizontal snap currently in force in the MSEG editor, 0 for none.
        """
    @hSnap.setter
    def hSnap(self, arg1: float) -> None:
        ...
    @property
    def hSnapDefault(self) -> float:
        """
        Horizontal snap the MSEG editor returns to when snap is toggled on.
        """
    @hSnapDefault.setter
    def hSnapDefault(self, arg1: float) -> None:
        ...
    @property
    def loopEnd(self) -> int:
        """
        Point the loop runs to, from -1 to segmentCount - 1, or surgepy.constants.mseg_unset to loop the whole MSEG.
        """
    @loopEnd.setter
    def loopEnd(self, arg1: int) -> None:
        ...
    @property
    def loopMode(self) -> int:
        """
        One of surgepy.constants.mseg_loop_off, mseg_loop_on or mseg_loop_gated.
        """
    @loopMode.setter
    def loopMode(self, arg1: int) -> None:
        ...
    @property
    def loopStart(self) -> int:
        """
        Point the loop returns to, from 0 to segmentCount, or surgepy.constants.mseg_unset to loop the whole MSEG.
        """
    @loopStart.setter
    def loopStart(self, arg1: int) -> None:
        ...
    @property
    def segmentCount(self) -> int:
        """
        How many segments this MSEG has.
        """
    @property
    def segments(self) -> list[SurgeMSEGSegment]:
        """
        The active segments, in order. Each one is a live reference into this MSEG.
        """
    @property
    def totalDuration(self) -> float:
        """
        Length of every segment added up. Always 1 in LFO edit mode.
        """
    @property
    def vSnap(self) -> float:
        """
        Vertical snap currently in force in the MSEG editor, 0 for none.
        """
    @vSnap.setter
    def vSnap(self, arg1: float) -> None:
        ...
    @property
    def vSnapDefault(self) -> float:
        """
        Vertical snap the MSEG editor returns to when snap is toggled on.
        """
    @vSnapDefault.setter
    def vSnapDefault(self, arg1: float) -> None:
        ...
class SurgeMSEGSegment:
    def __repr__(self) -> str:
        ...
    @property
    def cpduration(self) -> float:
        """
        Control point position along this segment, from 0 to 1.
        """
    @cpduration.setter
    def cpduration(self, arg1: float) -> None:
        ...
    @property
    def cpv(self) -> float:
        """
        Control point value, from -1 to 1. What it does depends on the segment type.
        """
    @cpv.setter
    def cpv(self, arg1: float) -> None:
        ...
    @property
    def duration(self) -> float:
        """
        Length of this segment, in beats when the LFO is tempo synced and in seconds otherwise.
        """
    @duration.setter
    def duration(self, arg1: float) -> None:
        ...
    @property
    def index(self) -> int:
        """
        Position of this segment in its MSEG.
        """
    @property
    def invertDeform(self) -> bool:
        """
        Is the LFO's Deform parameter inverted on this segment?
        """
    @invertDeform.setter
    def invertDeform(self, arg1: bool) -> None:
        ...
    @property
    def retriggerAEG(self) -> bool:
        """
        Does reaching this segment retrigger the amplitude envelope?
        """
    @retriggerAEG.setter
    def retriggerAEG(self, arg1: bool) -> None:
        ...
    @property
    def retriggerFEG(self) -> bool:
        """
        Does reaching this segment retrigger the filter envelope?
        """
    @retriggerFEG.setter
    def retriggerFEG(self, arg1: bool) -> None:
        ...
    @property
    def type(self) -> int:
        """
        Curve of this segment, one of the surgepy.constants.mseg_seg_ values.
        """
    @type.setter
    def type(self, arg1: int) -> None:
        ...
    @property
    def useDeform(self) -> bool:
        """
        Does the LFO's Deform parameter apply to this segment?
        """
    @useDeform.setter
    def useDeform(self, arg1: bool) -> None:
        ...
    @property
    def v0(self) -> float:
        """
        Value this segment starts at, from -1 to 1. A segment ends at the value the next one starts at, or at the MSEG's endValue for the last one.
        """
    @v0.setter
    def v0(self, arg1: float) -> None:
        ...
class SurgeModRouting:
    def __repr__(self) -> str:
        ...
    def getDepth(self) -> float:
        ...
    def getDest(self) -> SurgeNamedParamId:
        ...
    def getNormalizedDepth(self) -> float:
        ...
    def getSource(self) -> SurgeModSource:
        ...
    def getSourceIndex(self) -> int:
        ...
    def getSourceScene(self) -> int:
        ...
class SurgeModSource:
    def __repr__(self) -> str:
        ...
    def getModSource(self) -> int:
        ...
    def getName(self) -> str:
        ...
class SurgeNamedParamId:
    def __repr__(self) -> str:
        ...
    def getId(self) -> SurgeSynthesizer_ID:
        ...
    def getName(self) -> str:
        ...
class SurgeSynthesizer:
    mpeEnabled: bool
    tuningApplicationMode: ...
    def __repr__(self) -> str:
        ...
    def allNotesOff(self) -> None:
        """
        Turn off all playing notes
        """
    def canBeAbsolute(self, param: SurgePyNamedParam) -> bool:
        """
        Can this parameter be switched to absolute mode?
        """
    def canDeactivate(self, param: SurgePyNamedParam) -> bool:
        """
        Can this parameter be deactivated?
        """
    def canDeform(self, param: SurgePyNamedParam) -> bool:
        """
        Does this parameter have deform options?
        """
    def canExtend(self, param: SurgePyNamedParam) -> bool:
        """
        Can this parameter have an extended range?
        """
    def canPortamento(self, param: SurgePyNamedParam) -> bool:
        """
        Does this parameter have portamento options?
        """
    def canTempoSync(self, param: SurgePyNamedParam) -> bool:
        """
        Can this parameter be tempo synced?
        """
    def channelAftertouch(self, channel: int, value: int) -> None:
        """
        Send the channel aftertouch MIDI message
        """
    def channelController(self, channel: int, cc: int, value: int) -> None:
        """
        Set MIDI controller on channel to value
        """
    def checkFormula(self, scene: int, lfo: int) -> str:
        """
        Compile the formula modulator of an LFO in a scene, returning the error it reports, or an empty string if it runs.
        """
    def createMultiBlock(self, blockCapacity: int) -> numpy.ndarray[numpy.float32]:
        """
        Create a numpy array suitable to hold up to b blocks of Surge XT processing in processMultiBlock
        """
    def createSynthSideId(self, arg0: int) -> SurgeSynthesizer_ID:
        ...
    def fromSynthSideId(self, arg0: int, arg1: SurgeSynthesizer_ID) -> bool:
        ...
    def getAbsolute(self, param: SurgePyNamedParam) -> bool:
        """
        Is this parameter in absolute mode?
        """
    def getAllModRoutings(self) -> dict:
        """
        Get the entire modulation matrix for this instance.
        """
    def getBlockSize(self) -> int:
        ...
    def getControlGroup(self, entry: int) -> SurgePyControlGroup:
        """
        Gather the parameters groups for a surge.constants.cg_ control group
        """
    def getDeactivated(self, param: SurgePyNamedParam) -> bool:
        """
        Is this parameter deactivated?
        """
    def getDeform(self, param: SurgePyNamedParam) -> int:
        """
        The deform type of this parameter, as an integer whose meaning depends on the parameter.
        """
    def getExtend(self, param: SurgePyNamedParam) -> bool:
        """
        Is this parameter in extended range mode?
        """
    def getFactoryDataPath(self) -> str:
        ...
    def getFormula(self, scene: int, lfo: int) -> str:
        """
        The Lua body of the formula modulator of an LFO in a scene.
        """
    def getMSEG(self, scene: int, lfo: int) -> SurgePyMSEG:
        """
        The MSEG of an LFO in a scene, as a live reference: editing the object returned here edits the patch. Every LFO has one, but it is only saved with the patch while that LFO's shape is surgepy.constants.lt_mseg.
        """
    def getModDepth01(self, targetParameter: SurgePyNamedParam, modulationSource: SurgePyModSource, scene: int = 0, index: int = 0) -> float:
        """
        Get the modulation depth from a source to a parameter.
        """
    def getModSource(self, modId: int) -> SurgePyModSource:
        """
        Given a constant from surge.constants.ms_*, provide a modulator object
        """
    def getNumInputs(self) -> int:
        ...
    def getNumOutputs(self) -> int:
        ...
    def getOutput(self) -> numpy.ndarray[numpy.float32]:
        """
        Retrieve the internal output buffer as a 2 * BLOCK_SIZE numpy array.
        """
    def getParamDef(self, arg0: SurgePyNamedParam) -> float:
        """
        Parameter default value, as a float
        """
    def getParamDisplay(self, arg0: SurgePyNamedParam) -> str:
        """
        Parameter value display (stringified and formatted)
        """
    def getParamInfo(self, arg0: SurgePyNamedParam) -> str:
        """
        Parameter value info (formatted)
        """
    def getParamMax(self, arg0: SurgePyNamedParam) -> float:
        """
        Parameter maximum value, as a float
        """
    def getParamMin(self, arg0: SurgePyNamedParam) -> float:
        """
        Parameter minimum value, as a float.
        """
    def getParamVal(self, arg0: SurgePyNamedParam) -> float:
        """
        Parameter current value in this Surge XT instance, as a float
        """
    def getParamValType(self, arg0: SurgePyNamedParam) -> str:
        """
        Parameter types float, int or bool are supported
        """
    def getParameterName(self, arg0: SurgeSynthesizer_ID) -> str:
        """
        Given a parameter, return its name as displayed by the synth.
        """
    def getPatch(self) -> dict:
        """
        Get a Python dictionary with the Surge XT parameters laid out in the logical patch format
        """
    def getPortamentoOptions(self, param: SurgePyNamedParam) -> dict:
        """
        The portamento options of this parameter, as a dictionary with the keys 'constantRate', 'glissando', 'retrigger' and 'curve'.
        """
    def getSampleRate(self) -> float:
        ...
    def getTempoSync(self, param: SurgePyNamedParam) -> bool:
        """
        Is this parameter tempo synced?
        """
    def getUserDataPath(self) -> str:
        ...
    def isActiveModulation(self, targetParameter: SurgePyNamedParam, modulationSource: SurgePyModSource, scene: int = 0, index: int = 0) -> bool:
        """
        Is there an established modulation between target and source?
        """
    def isBipolarModulation(self, modulationSource: SurgePyModSource) -> bool:
        """
        Is the given modulation source bipolar?
        """
    def isValidModulation(self, targetParameter: SurgePyNamedParam, modulationSource: SurgePyModSource) -> bool:
        """
        Is it possible to modulate between target and source?
        """
    def loadKBMFile(self, arg0: str) -> None:
        """
        Load a KBM mapping file and apply tuning to this instance
        """
    def loadPatch(self, path: str) -> bool:
        """
        Load a Surge XT .fxp patch from the file system.
        """
    def loadSCLFile(self, arg0: str) -> None:
        """
        Load an SCL tuning file and apply tuning to this instance
        """
    def loadWavetable(self, scene: int, osc: int, path: str) -> bool:
        """
        Load a wavetable file directly into a scene and oscillator immediately on this thread.
        """
    def pitchBend(self, channel: int, bend: int) -> None:
        """
        Set the pitch bend value on channel ch
        """
    def playNote(self, channel: int, midiNote: int, velocity: int, detune: int = 0) -> None:
        """
        Trigger a note on this Surge XT instance.
        """
    def polyAftertouch(self, channel: int, key: int, value: int) -> None:
        """
        Send the poly aftertouch MIDI message
        """
    def process(self) -> None:
        """
        Run Surge XT for one block and update the internal output buffer.
        """
    def processMultiBlock(self, val: numpy.ndarray[numpy.float32], startBlock: int = 0, nBlocks: int = -1) -> None:
        """
        Run the Surge XT engine for multiple blocks, updating the value in the numpy array. Either populate the
        entire array, or starting at startBlock position in the output, populate nBlocks.
        """
    def processMultiBlockWithInput(self, inVal: numpy.ndarray[numpy.float32], outVal: numpy.ndarray[numpy.float32], startBlock: int = 0, nBlocks: int = -1) -> None:
        """
        Run the Surge XT engine for multiple blocks using the input numpy array,
        updating the value in the output numpy array. Either populate the
        entire array, or starting at startBlock position in the output, populate nBlocks.
        """
    def releaseNote(self, channel: int, midiNote: int, releaseVelocity: int = 0) -> None:
        """
        Release a note on this Surge XT instance.
        """
    def remapToStandardKeyboard(self) -> None:
        """
        Return to standard C-centered keyboard mapping
        """
    def retuneToStandardScale(self) -> None:
        """
        Return this instance to 12-TET Scale
        """
    def retuneToStandardTuning(self) -> None:
        """
        Return this instance to 12-TET Concert Keyboard Mapping
        """
    def savePatch(self, path: str) -> None:
        """
        Save the current state of Surge XT to an .fxp file.
        """
    def saveWavetable(self, scene: int, osc: int, path: str) -> bool:
        """
        Save the wavetable of a scene and oscillator to a .wt file, immediately on this thread.
        """
    def setAbsolute(self, param: SurgePyNamedParam, toThis: bool) -> None:
        """
        Set the absolute mode of a parameter.
        """
    def setDeactivated(self, param: SurgePyNamedParam, toThis: bool) -> None:
        """
        Set the deactivated state of a parameter.
        """
    def setDeform(self, param: SurgePyNamedParam, toThis: int) -> None:
        """
        Set the deform type of a parameter.
        """
    def setExtend(self, param: SurgePyNamedParam, toThis: bool) -> None:
        """
        Set the extended range mode of a parameter, rescaling its value to the new range.
        """
    def setFormula(self, scene: int, lfo: int, formula: str) -> None:
        """
        Set the Lua body of the formula modulator of an LFO in a scene. The formula is not compiled here - use checkFormula() for that - and is only saved with the patch while that LFO's shape is surgepy.constants.lt_formula.
        """
    def setMSEG(self, scene: int, lfo: int, mseg: SurgePyMSEG) -> None:
        """
        Copy an MSEG onto the MSEG of an LFO in a scene, raising if the source doesn't pass validateMSEG().
        """
    def setModDepth01(self, targetParameter: SurgePyNamedParam, modulationSource: SurgePyModSource, depth: float, scene: int = 0, index: int = 0) -> None:
        """
        Set a modulation to a given depth
        """
    def setParamVal(self, param: SurgePyNamedParam, toThis: float) -> None:
        """
        Set a parameter value
        """
    def setPortamentoOptions(self, param: SurgePyNamedParam, constantRate: bool | None = None, glissando: bool | None = None, retrigger: bool | None = None, curve: int | None = None) -> None:
        """
        Set the portamento options of a parameter. Options which are not given are left alone. The curve is one of surgepy.constants.porta_log, porta_lin or porta_exp.
        """
    def setTempoSync(self, param: SurgePyNamedParam, toThis: bool) -> None:
        """
        Set the tempo sync mode of a parameter.
        """
class SurgeSynthesizer_ID:
    def __init__(self) -> None:
        ...
    def __repr__(self) -> str:
        ...
    def getSynthSideId(self) -> int:
        ...
class TuningApplicationMode:
    """
    Members:
    
      RETUNE_ALL
    
      RETUNE_MIDI_ONLY
    """
    RETUNE_ALL: typing.ClassVar[TuningApplicationMode]  # value = <TuningApplicationMode.RETUNE_ALL: 0>
    RETUNE_MIDI_ONLY: typing.ClassVar[TuningApplicationMode]  # value = <TuningApplicationMode.RETUNE_MIDI_ONLY: 1>
    __members__: typing.ClassVar[dict[str, TuningApplicationMode]]  # value = {'RETUNE_ALL': <TuningApplicationMode.RETUNE_ALL: 0>, 'RETUNE_MIDI_ONLY': <TuningApplicationMode.RETUNE_MIDI_ONLY: 1>}
    def __eq__(self, other: typing.Any) -> bool:
        ...
    def __getstate__(self) -> int:
        ...
    def __hash__(self) -> int:
        ...
    def __index__(self) -> int:
        ...
    def __init__(self, value: int) -> None:
        ...
    def __int__(self) -> int:
        ...
    def __ne__(self, other: typing.Any) -> bool:
        ...
    def __repr__(self) -> str:
        ...
    def __setstate__(self, state: int) -> None:
        ...
    def __str__(self) -> str:
        ...
    @property
    def name(self) -> str:
        ...
    @property
    def value(self) -> int:
        ...
def createSurge(sampleRate: float) -> SurgeSynthesizer:
    """
    Create a Surge XT instance
    """
def getVersion() -> str:
    """
    Get the version of Surge XT
    """
def validateMSEG(mseg: SurgeMSEG) -> list[str]:
    """
    Everything structurally wrong with an MSEG, as a list of descriptions which is empty when it is valid.
    """

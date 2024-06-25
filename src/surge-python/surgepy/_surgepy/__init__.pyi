"""
Python bindings for Surge XT Synthesizer
"""
from __future__ import annotations
import numpy
import typing
from . import constants
__all__ = ['SurgeControlGroup', 'SurgeControlGroupEntry', 'SurgeModRouting', 'SurgeModSource', 'SurgeNamedParamId', 'SurgeSynthesizer', 'SurgeSynthesizer_ID', 'TuningApplicationMode', 'constants', 'createSurge', 'getVersion']
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
    def channelAftertouch(self, channel: int, value: int) -> None:
        """
        Send the channel aftertouch MIDI message
        """
    def channelController(self, channel: int, cc: int, value: int) -> None:
        """
        Set MIDI controller on channel to value
        """
    def createMultiBlock(self, blockCapacity: int) -> numpy.ndarray[numpy.float32]:
        """
        Create a numpy array suitable to hold up to b blocks of Surge XT processing in processMultiBlock
        """
    def createSynthSideId(self, arg0: int) -> SurgeSynthesizer_ID:
        ...
    def fromSynthSideId(self, arg0: int, arg1: SurgeSynthesizer_ID) -> bool:
        ...
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
    def getFactoryDataPath(self) -> str:
        ...
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
    def getSampleRate(self) -> float:
        ...
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
    def setModDepth01(self, targetParameter: SurgePyNamedParam, modulationSource: SurgePyModSource, depth: float, scene: int = 0, index: int = 0) -> None:
        """
        Set a modulation to a given depth
        """
    def setParamVal(self, param: SurgePyNamedParam, toThis: float) -> None:
        """
        Set a parameter value
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

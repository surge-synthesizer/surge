# Build and include libs/midifile

set(LIB_MIDIFILE_DIR libs/midifile)
set(LIB_MIDIFILE_SRC_DIR "${LIB_MIDIFILE_DIR}/src-library/")
set(LIB_MIDIFILE_INCLUDES "${LIB_MIDIFILE_DIR}/include/")

set(LIB_MIDIFILE_SOURCES
   ${LIB_MIDIFILE_SRC_DIR}/Binasc.cpp
   ${LIB_MIDIFILE_SRC_DIR}/MidiEvent.cpp
   ${LIB_MIDIFILE_SRC_DIR}/MidiEventList.cpp
   ${LIB_MIDIFILE_SRC_DIR}/MidiFile.cpp
   ${LIB_MIDIFILE_SRC_DIR}/MidiMessage.cpp
)

# If we upgrade to cmake 3.12 we could do this instead
# list(TRANSFORM LIB_MIDIFILE_SOURCES PREPEND ${LIB_MIDIFILE_SRC_DIR})


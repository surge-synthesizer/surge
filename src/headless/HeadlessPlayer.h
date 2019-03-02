#include "SurgeSynthesizer.h"
#include <vector>

namespace Surge
{
namespace Headless
{
namespace Player
{

/*
** A variety of player routines all with the same signature of "SurgeSynth *, vector<float>&"
** plays the type in the name and outputs the data into the float vector as itnerleaved stero
** (so l1 r1 l2 r3 l2 r3 etc...)
*/

void play120BPMCMajorQuarterNoteScale(SurgeSynthesizer* synth, std::vector<float>& output);

} // namespace Player
} // namespace Headless
} // namespace Surge

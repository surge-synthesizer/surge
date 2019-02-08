#include "SurgeSynthesizer.h"

namespace Surge
{
namespace Headless
{

/**
 * createSurge
 *
 * Create an instance of a SurgeSynthesizer object correctly configured
 * to run in headless mode
 */
SurgeSynthesizer* createSurge();

/**
 * loadPatchByIndex
 *
 * Appropriately loads the surge patch at "index", returning false if
 * no patch is available
 */
bool loadPatchByIndex(SurgeSynthesizer* s, int index);

} // namespace Headless
} // namespace Surge

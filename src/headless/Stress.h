/*
** These are routines that stress single surges or buckets of surges. I mostly
** wrote them while chasing click-n-pop
*/

#pragma once

#include "HeadlessUtils.h"

namespace Surge
{
namespace Headless
{

/*
** Create and destroy a lot of surges with a random patch and play a major scale on each
*/
void createAndDestroyWithScaleAndRandomPatch(int timesToTry = 5000);

} // namespace Headless
} // namespace Surge

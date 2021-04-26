/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#pragma once

#include <string>
#include <functional>
#include "filesystem/import.h"

class SurgeStorage;

namespace Surge
{
namespace Storage
{
#if BUILD_DEFERRED_ASSET_LOADER
class DeferredAssetLoader
{
  public:
    explicit DeferredAssetLoader(SurgeStorage *s);

    typedef std::string url_t;

    bool hasCachedCopyOf(const url_t &url);
    fs::path pathOfCachedCopy(const url_t &url);
    void retrieveSingleAsset(const url_t &url, std::function<void(const url_t &)> onRetrieved,
                             std::function<void(const url_t &, const std::string &msg)> onError);

    /*
     * For Surge 1.9 and using this for wavetables we will need an API something like this:
     *
    void retrieveMultipleAssets(const std::vector<url_t> & assets,
                                std::function<void()> onAllRetrieved,
                                std::function<void(const std::vector<url_t>)> onSomeFailed);
    */

  private:
    fs::path cacheDir;
};
#endif

} // namespace Storage
} // namespace Surge
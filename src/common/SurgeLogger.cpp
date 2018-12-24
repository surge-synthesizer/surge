#include "SurgeLogger.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_sinks.h"

#include "spdlog/logger.h"
#include "spdlog/async.h"
#include <memory>
#include <filesystem.h>

/*
** This file exists to have a single global static which is initialized at library open time
** thereby making sure that the log is set up as we want when it runs. 
**
** Two parts. The "init" function below sets up the logger in the OS reasonable way.
** For now it is stderr+~/Library/Logs on macos and stderr only on windows and linux.
**
** The second part is the static variable which is initialized with the function call
*/

namespace SurgeLogInternal
{

typedef std::shared_ptr< spdlog::logger > log_t;
namespace fs = std::experimental::filesystem;

log_t internalSurgeLogInit()
{
    std::vector<spdlog::sink_ptr> sinks;
    
    /*
    ** Where to put the logfile is actually OS dependent. 
    ** The approach we take here is to add a pair of sinks
    ** to the logger, the first to stderr
    */
    auto stderrLogger = std::make_shared<spdlog::sinks::stderr_sink_mt>();
    sinks.push_back( stderrLogger );
    
    /*
    ** And if someone has implemented this for their OS, the second to a standard
    ** logfile location
    */


#if MAC 
    char lfb[ PATH_MAX ];
    sprintf(lfb, "%s/Library/Logs/Surge.log", getenv("HOME"));
    auto fileSink = make_shared<spdlog::sinks::basic_file_sink_mt> (lfb);
    sinks.push_back( fileSink );
#endif
    
#if WINDOWS
    // FIXME: Where's the default log location on windows?
#endif
    
#if __linux__
    char lfb[ PATH_MAX ];
    sprintf(lfb, "%s/.cache/Surge", getenv("HOME"));
    fs::create_directories(fs::path(lfb));
    
    sprintf(lfb, "%s/.cache/Surge/Surge.log", getenv("HOME"));
    auto fileSink = make_shared<spdlog::sinks::basic_file_sink_mt> (lfb);
    
    sinks.push_back( fileSink );
#endif
    
    
    auto logger = std::make_shared<spdlog::logger>("surge", sinks.begin(), sinks.end());
    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);
    
    return logger;
}
    
static log_t internal_surge_log_instance = internalSurgeLogInit();

}

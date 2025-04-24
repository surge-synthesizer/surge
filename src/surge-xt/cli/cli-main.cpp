/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_devices/juce_audio_devices.h>
#if defined(_M_ARM64EC)
#include <juce_gui_extra/juce_gui_extra.h>
#endif

#include <iostream>
#include <CLI11/CLI11.hpp>

#include "version.h"

#include "SurgeSynthProcessor.h"

#if JUCE_MAC
namespace juce
{
extern void initialiseNSApplication();
}
extern void objCShutdown(); // in cli-mac-helpers.mm
#endif

// This tells us to keep processing
std::atomic<bool> continueLoop{true};

// Thanks to
// https://stackoverflow.com/questions/16077299/how-to-print-current-time-with-milliseconds-using-c-c11
std::string logTimestamp()
{
    using namespace std::chrono;
    using clock = system_clock;

    const auto current_time_point{clock::now()};
    const auto current_time{clock::to_time_t(current_time_point)};
    const auto current_localtime{*std::localtime(&current_time)};
    const auto current_time_since_epoch{current_time_point.time_since_epoch()};
    const auto current_milliseconds{duration_cast<milliseconds>(current_time_since_epoch).count() %
                                    1000};

    std::ostringstream stream;
    stream << std::put_time(&current_localtime, "%T") << "." << std::setw(3) << std::setfill('0')
           << current_milliseconds;
    return stream.str();
}

enum LogLevels
{
    NONE = 0,
    BASIC = 1,
    VERBOSE = 2
};
int logLevel{BASIC};
#define LOG(lev, x)                                                                                \
    {                                                                                              \
        if (logLevel >= lev)                                                                       \
        {                                                                                          \
            std::cout << logTimestamp() << " - " << x << std::endl;                                \
        }                                                                                          \
    }
#define PRINT(x) LOG(BASIC, x);
#define PRINTERR(x) LOG(BASIC, "Error: " << x);

#ifndef WINDOWS
#include <signal.h>

[[noreturn]] void onTerminate() noexcept
{
    auto e = std::current_exception();

    if (e)
    {
        try
        {
            rethrow_exception(e);
        }
        catch (...)
        {
            PRINTERR("Exception occurred");
        }
    }

    std::_Exit(continueLoop);
}

void ctrlc_callback_handler(int signum)
{
    std::cout << "\n";
    LOG(BASIC, "SIGINT (Ctrl-C) detected. Shutting down CLI...");
    continueLoop = false;
    juce::MessageManager::getInstance()->stopDispatchLoop();

    std::set_terminate(onTerminate);
    std::terminate();
}
#endif

void listAudioDevices()
{
    juce::AudioDeviceManager manager;

    juce::OwnedArray<juce::AudioIODeviceType> types;
    manager.createAudioDeviceTypes(types);

    for (int i = 0; i < types.size(); ++i)
    {
        juce::String typeName(types[i]->getTypeName());

        types[i]->scanForDevices(); // This must be called before getting the list of devices

        juce::StringArray deviceNames(types[i]->getDeviceNames()); // This will now return a list of
                                                                   // available devices of this type

        for (int j = 0; j < deviceNames.size(); ++j)
        {
            PRINT("Output Audio Device: [" << i << "." << j << "] : " << typeName << "."
                                           << deviceNames[j]);
        }

        juce::StringArray inputDeviceNames(
            types[i]->getDeviceNames(true)); // This will now return a list of
                                             // available devices of this type

        for (int j = 0; j < inputDeviceNames.size(); ++j)
        {
            PRINT("Input Audio Device: [" << i << "." << j << "] : " << typeName << "."
                                          << inputDeviceNames[j]);
        }
    }
}

void listMidiDevices()
{
    juce::MessageManager::getInstance()->setCurrentThreadAsMessageThread();

    auto items = juce::MidiInput::getAvailableDevices();
    for (int i = 0; i < items.size(); ++i)
    {
        PRINT("MIDI Device: [" << i << "] : " << items[i].name);
    }
    juce::MessageManager::deleteInstance();
}

struct SurgePlayback : juce::MidiInputCallback, juce::AudioIODeviceCallback
{
    std::unique_ptr<SurgeSynthProcessor> proc;
    SurgePlayback()
    {
        juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
        proc = std::make_unique<SurgeSynthProcessor>();
        proc->standaloneTempo = 120;
    }

    static constexpr int midiBufferSz{4096}, midiBufferSzMask{midiBufferSz - 1};
    std::array<juce::MidiMessage, midiBufferSz> midiBuffer;
    std::atomic<int> midiWP{0}, midiRP{0};
    void handleIncomingMidiMessage(juce::MidiInput *source,
                                   const juce::MidiMessage &message) override
    {
        midiBuffer[midiWP] = message;
        midiWP = (midiWP + 1) & midiBufferSzMask;
    }

    int pos = BLOCK_SIZE;
    void
    audioDeviceIOCallbackWithContext(const float *const *inputChannelData, int numInputChannels,
                                     float *const *outputChannelData, int numOutputChannels,
                                     int numSamples,
                                     const juce::AudioIODeviceCallbackContext &context) override
    {
        proc->surge->audio_processing_active = true;
        proc->processBlockOSC();
        proc->processBlockPlayhead();

        if (numInputChannels > 0)
        {
            proc->surge->process_input = true;
        }

        int inputR = numInputChannels > 1 ? 1 : 0;

        for (int i = 0; i < numSamples; ++i)
        {
            if (pos >= BLOCK_SIZE)
            {
                int mw = midiWP;

                while (mw != midiRP)
                {
                    proc->applyMidi(midiBuffer[midiRP]);
                    midiRP = (midiRP + 1) & midiBufferSzMask;
                }
                proc->surge->process();

                pos = 0;
            }
            if (numInputChannels > 0)
            {
                proc->surge->input[0][pos] = inputChannelData[0][i];
                proc->surge->input[1][pos] = inputChannelData[inputR][i];
            }
            outputChannelData[0][i] = proc->surge->output[0][pos];
            outputChannelData[1][i] = proc->surge->output[1][pos];
            pos++;
        }
    }

    void audioDeviceStopped() override { proc->surge->audio_processing_active = false; }
    void audioDeviceAboutToStart(juce::AudioIODevice *device) override
    {
        LOG(BASIC, "Audio Starting      : Sample Rate "
                       << (int)device->getCurrentSampleRate() << " Hz, Buffer Size "
                       << device->getCurrentBufferSizeSamples() << " samples");
        proc->surge->setSamplerate(device->getCurrentSampleRate());
    }
};

void isQuitPressed()
{
    std::string res;

    while (!(std::cin.eof() || res == "quit"))
    {
        std::cout << "\nSurge XT CLI is running. Type 'quit' or Ctrl+D to stop\n> ";
        std::cin >> res;
    }
    continueLoop = false;
    juce::MessageManager::getInstance()->stopDispatchLoop();
}

int main(int argc, char **argv)
{
    // juce::ConsoleApplication is just such a mess.
    CLI::App app("..:: Surge XT CLI - Command Line player for Surge XT ::..");

    app.set_version_flag("--version", Surge::Build::FullVersionStr);

    bool listDevices{false};
    app.add_flag("-l,--list-devices", listDevices,
                 "List all devices available on this system, then quit.");

    std::string audioInterface{};
    app.add_flag("--audio-interface", audioInterface,
                 "Select an audio interface, using index (like '0.2') as shown in list-devices.");

    std::string audioPorts{};
    app.add_flag(
        "--audio-ports", audioPorts,
        "Select the ports to address in the audio interface. '0,1' means first pair, zero based.");

    std::string audioInputInterface{};
    app.add_flag(
        "--audio-input-interface", audioInputInterface,
        "Select an audio input interface, using index (like '0.2') as shown in list-devices.");

    std::string audioInputPorts{};
    app.add_flag("--audio-input-ports", audioInputPorts,
                 "Select the ports to address in the audio interface. '0,1' means first pair, zero "
                 "based. For a single channel just use a single number");

    bool allMidi{false};
    app.add_flag("--all-midi-inputs", allMidi, "Bind all available MIDI inputs to the synth.");

    int midiInput{};
    app.add_flag("--midi-input", midiInput,
                 "Select a single MIDI input using the index from list-devices. Overrides "
                 "--all-midi-inputs.")
        ->default_val("-1"); // a value of -1 means we skip loading devices altogether.

    int oscInputPort{0};
    app.add_flag("--osc-in-port", oscInputPort,
                 "Port for OSC input. If not specified, OSC will not be used.");

    int oscOutputPort{0};
    app.add_flag("--osc-out-port", oscOutputPort,
                 "Port for OSC output. If not specified, OSC input will be used and required.");

    std::string oscOutputIPAddr{"127.0.0.1"};
    app.add_flag(
        "--osc-out-ipaddr", oscOutputIPAddr,
        "IP Address for OSC output. If not specified, local host will be used (127.0.0.1).");

    int sampleRate{0};
    app.add_flag("--sample-rate", sampleRate,
                 "Sample rate for audio output. If not specified, system default will be used.");

    int bufferSize{0};
    app.add_flag("--buffer-size", bufferSize,
                 "Buffer size for audio output. If not specified, system default will be used.");

    std::string initPatch{};
    app.add_flag("--init-patch", initPatch, "Choose this file path as the initial patch.");

    bool noStdIn{false};
    app.add_flag("--no-stdin", noStdIn,
                 "Do not assume stdin and do not poll keyboard for quit or ctrl-d. Useful for "
                 "daemon modes.");

    bool mpeEnable{false};
    app.add_flag("--mpe-enable", mpeEnable, "Enable MPE mode on this instance of Surge XT CLI");

    int mpeBendRange{0};
    app.add_flag("--mpe-pitch-bend-range", mpeBendRange,
                 "MPE Pitch Bend Range in semitones; 0 for default");

    CLI11_PARSE(app, argc, argv);

    if (listDevices)
    {
        listAudioDevices();
        listMidiDevices();
        exit(0);
    }

    auto *mm = juce::MessageManager::getInstance();
    mm->setCurrentThreadAsMessageThread();

    /*
     * This is the default runloop. Basically this main thread acts as the message queue
     */
    auto engine = std::make_unique<SurgePlayback>();
    if (!initPatch.empty())
    {
        if (engine->proc->surge->loadPatchByPath(initPatch.c_str(), -1, "Loaded Patch"))
        {
            LOG(BASIC, "Loaded patch        : " << initPatch);
        }
        else
        {
            LOG(BASIC, "Failed to load patch:" << initPatch << "!");
        }
    }

    if (mpeEnable)
    {
        LOG(BASIC, "MPE Status          : Enabled");
        engine->proc->surge->mpeEnabled = true;

        if (mpeBendRange > 0)
        {
            LOG(BASIC, "MPE Bend Range      : " << mpeBendRange);
            engine->proc->surge->storage.mpePitchBendRange = mpeBendRange;
        }
    }
    else
    {
        engine->proc->surge->mpeEnabled = false; // the default
    }

    auto midiDevices = juce::MidiInput::getAvailableDevices();
    std::vector<std::unique_ptr<juce::MidiInput>> midiInputs;

    // Only try to open a MIDI device when explicitly requested.
    if (midiInput >= 0)
    {
        if (midiInput >= midiDevices.size())
        {
            PRINTERR("Invalid MIDI input device number!");
            exit(1);
        }

        auto vmini = midiDevices[midiInput];
        auto inp = juce::MidiInput::openDevice(vmini.identifier, engine.get());
        if (!inp)
        {
            PRINTERR("Unable to open MIDI device " << vmini.name << "!");
            exit(1);
        }
        midiInputs.push_back(std::move(inp));
        LOG(BASIC, "Opened MIDI Input   : [" << vmini.name << "] ");
    }

    // Warn about mixing MIDI input commands
    if (midiInput >= 0 && allMidi)
    {
        LOG(BASIC, "WARNING: Selecting a single MIDI device overrides --all-midi-inputs!")
    }

    // Requesting all devices always works, even if no devices are found.
    // Explicitly requesting a single MIDI device means we skip this.
    if (allMidi && midiInput < 0)
    {
        LOG(BASIC, "Binding to all MIDI inputs... Found " << midiDevices.size() << " device"
                                                          << (midiDevices.size() == 1 ? "" : "s")
                                                          << "!");
        for (auto &vmini : midiDevices)
        {
            auto inp = juce::MidiInput::openDevice(vmini.identifier, engine.get());
            if (!inp)
            {
                PRINTERR("Unable to open MIDI device " << vmini.name << "!");
                exit(1);
            }
            midiInputs.push_back(std::move(inp));
            LOG(BASIC, "Opened MIDI Input   : [" << vmini.name << "] ");
        }
    }

    auto manager = std::make_unique<juce::AudioDeviceManager>();
    juce::OwnedArray<juce::AudioIODeviceType> types;
    manager->createAudioDeviceTypes(types);

    int audioTypeIndex = 0;
    int audioDeviceIndex = 0;
    if (audioInterface.empty())
    {
        types[audioTypeIndex]->scanForDevices();
        audioDeviceIndex = types[audioTypeIndex]->getDefaultDeviceIndex(false);
        LOG(BASIC, "Audio device is not specified! Using system default.");
    }
    else
    {
        auto p = audioInterface.find('.');
        if (p == std::string::npos)
        {
            PRINTERR(
                "Audio interface argument must be of form a.b, as per --list-devices. You gave "
                << audioInterface << ".");
            exit(3);
        }
        else
        {
            auto da = std::atoi(audioInterface.substr(0, p).c_str());
            auto dt = std::atoi(audioInterface.substr(p + 1).c_str());
            audioTypeIndex = da;
            audioDeviceIndex = dt;

            if (da < 0 || da >= types.size())
            {
                PRINTERR("Audio type index must be in range 0 ... " << types.size() - 1 << "!");
                exit(4);
            }
        }
    }

    const auto &atype = types[audioTypeIndex];
    LOG(BASIC, "Audio driver type   : [" << atype->getTypeName() << "]")

    int inputTypeIndex = -1;
    int inputDeviceIndex = -1;
    if (!audioInputInterface.empty())
    {
        auto p = audioInputInterface.find('.');
        if (p == std::string::npos)
        {
            PRINTERR("Audio input interface argument must be of form a.b, as per --list-devices. "
                     "You gave "
                     << audioInputInterface << ".");
            exit(3);
        }
        else
        {
            auto da = std::atoi(audioInputInterface.substr(0, p).c_str());
            auto dt = std::atoi(audioInputInterface.substr(p + 1).c_str());
            inputTypeIndex = da;
            inputDeviceIndex = dt;

            if (da < 0 || da >= types.size())
            {
                PRINTERR("Audio type index must be in range 0 ... " << types.size() - 1 << "!");
                exit(4);
            }

            if (inputTypeIndex != audioTypeIndex)
            {
                PRINTERR(
                    "Right now, the type (a. or a.b) of the input and output must be the same");
                exit(5);
            }
        }
    }

    atype->scanForDevices(); // This must be called before getting the list of devices
    juce::StringArray deviceNames(atype->getDeviceNames()); // This will now return a list of

    if (audioDeviceIndex < 0 || audioDeviceIndex >= deviceNames.size())
    {
        PRINTERR("Audio device index must be in range 0 ... " << deviceNames.size() - 1 << "!");
    }

    auto iname = juce::String();
    if (inputDeviceIndex >= 0)
    {
        juce::StringArray inputDeviceNames(atype->getDeviceNames(true));
        iname = inputDeviceNames[inputDeviceIndex];
        LOG(BASIC, "Input device        : [" << iname << "]");
    }

    const auto &dname = deviceNames[audioDeviceIndex];

    LOG(BASIC, "Output device       : [" << dname << "]");
    std::unique_ptr<juce::AudioIODevice> device;
    device.reset(atype->createDevice(dname, iname));

    auto sr = device->getAvailableSampleRates();
    auto bs = device->getAvailableBufferSizes();

    if (sampleRate == 0)
    {
        auto candSampleRate = device->getCurrentSampleRate();
        for (auto s : sr)
        {
            if (s == candSampleRate)
                sampleRate = s;
        }
        sampleRate = sr[0];
    }
    else
    {
        auto candSampleRate = sampleRate;
        sampleRate = 0;
        for (auto s : sr)
            if (s == candSampleRate)
                sampleRate = s;
        if (sampleRate == 0)
        {
            LOG(BASIC, "Sample rate " << candSampleRate << " is not supported!");
            LOG(BASIC, "Your audio interface supports these sample rates:");
            for (auto s : sr)
            {
                LOG(BASIC, "   " << s);
            }
            exit(2);
        }
    }

    if (bufferSize == 0)
    {
        bufferSize = device->getDefaultBufferSize();
        // we just assume this is in the set
    }
    else
    {
        auto candBufferSize = bufferSize;
        bufferSize = 0;
        for (auto s : bs)
            if (s == candBufferSize)
                bufferSize = s;
        if (bufferSize == 0)
        {
            LOG(BASIC, "Buffer size " << bufferSize << " is not supported!");
            LOG(BASIC, "Your audio interface supports these sizes:");
            for (auto s : bs)
            {
                LOG(BASIC, "   " << s);
            }
        }
    }

    auto outputBitset = 3; // ports 0,1
    if (!audioPorts.empty())
    {
        auto p = audioPorts.find(',');
        if (p == std::string::npos)
        {
            PRINTERR("Audio ports argument must be of form L,R. You gave " << audioPorts << ".");
            exit(3);
        }
        else
        {
            auto dl = std::atoi(audioPorts.substr(0, p).c_str());
            auto dr = std::atoi(audioPorts.substr(p + 1).c_str());
            LOG(BASIC, "Binding to outputs  : L = " << dl << ", R = " << dr << "");
            outputBitset = (1 << (dl)) + (1 << (dr));
        }
    }

    // For now default input to the stereo or mono set
    auto inputBitset = 3;
    auto c = device->getInputChannelNames();
    if (c.size() == 1)
        inputBitset = 1;
    if (c.isEmpty())
        inputBitset = 0;

    if (!audioInputPorts.empty())
    {
        auto p = audioInputPorts.find(',');
        if (p == std::string::npos)
        {
            auto dl = std::atoi(audioInputPorts.c_str());
            LOG(BASIC, "Binding to input    : mono = " << dl);

            inputBitset = 1 << dl;
        }
        else
        {
            auto dl = std::atoi(audioInputPorts.substr(0, p).c_str());
            auto dr = std::atoi(audioInputPorts.substr(p + 1).c_str());
            LOG(BASIC, "Binding to inputs   : L = " << dl << ", R = " << dr << "");
            inputBitset = (1 << (dl)) + (1 << (dr));
        }
    }

    auto res = device->open(inputBitset, outputBitset, sampleRate, bufferSize);
    if (!res.isEmpty())
    {
        PRINTERR("Unable to open audio device: " << res << "!");
        exit(3);
    }

    device->start(engine.get());

    for (const auto &inp : midiInputs)
    {
        inp->start();
    }

    manager->addAudioCallback(engine.get());

    bool needsMessageLoop{false};
    if (oscInputPort > 0)
    {
        needsMessageLoop = true;
        LOG(BASIC, "Starting OSC input on " << oscInputPort);
        engine->proc->initOSCIn(oscInputPort);
        if (oscOutputPort > 0)
        {
            LOG(BASIC, "Starting OSC output on " << oscOutputPort);
            engine->proc->initOSCOut(oscOutputPort, oscOutputIPAddr);
        }
    }

    if (needsMessageLoop)
    {
        LOG(BASIC, "Running CLI with message loop");
    }
    else
    {
        LOG(BASIC, "Running CLI without message loop");
    }

    if (!noStdIn)
    {
        std::thread keyboardInputThread([]() { isQuitPressed(); });
        keyboardInputThread.detach();
    }
    else
    {
        LOG(BASIC, "Daemon mode without stdin. Send SIGINT or SIGTERM to cleanly stop execution.");
    }

#ifndef WINDOWS
    signal(SIGINT, ctrlc_callback_handler);
    signal(SIGTERM, ctrlc_callback_handler);
#endif

#if MAC
    if (needsMessageLoop)
    {
        LOG(BASIC, "Starting NSApp for MAC Loop");
        juce::initialiseNSApplication();
    }
#endif

    while (continueLoop)
    {
        if (needsMessageLoop)
        {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(25ms);
            mm->runDispatchLoop();
        }
        else
        {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(200ms);
        }
    }

    LOG(BASIC, "Shutting down CLI...");
#if JUCE_MAC
    if (needsMessageLoop)
    {
        LOG(BASIC, "Shutting down NSApp");
        objCShutdown();
    }
#endif

    // Handle interrupt and collect these in lambda to close if you bail out
    device->stop();
    device->close();
    for (const auto &inp : midiInputs)
    {
        inp->stop();
    }

    device.reset();
    manager.reset();
    juce::MessageManager::deleteInstance();
}

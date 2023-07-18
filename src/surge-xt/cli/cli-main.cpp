/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2023, various authors, as described in the GitHub
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

#include <iostream>
#include "version.h"
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_devices/juce_audio_devices.h>

void listAudioDevices(juce::AudioDeviceManager &manager)
{
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
            std::cout << "Audio Device: [" << i << "." << j << "] : " << typeName << "."
                      << deviceNames[j] << std::endl;
        }
    }
}

void listMidiDevices()
{
    auto items = juce::MidiInput::getAvailableDevices();
    for (int i = 0; i < items.size(); ++i)
    {
        std::cout << "MIDI Device: [" << i << "] : " << items[i].name
                  << " (identifier=" << items[i].identifier << ")" << std::endl;
    }
}

int main(int argc, char **argv)
{
    std::cout << "SurgeXT CLI\n    Surge Version  : " << Surge::Build::FullVersionStr
              << "\n    Build Time     : " << Surge::Build::BuildDate << " @ "
              << Surge::Build::BuildTime << "\n    JUCE Version   : " << std::hex << JUCE_VERSION
              << std::dec << std::endl;

    juce::MessageManager::getInstance()->setCurrentThreadAsMessageThread();
    juce::AudioDeviceManager manager;
    listAudioDevices(manager);
    listMidiDevices();

    juce::MessageManager::deleteInstance();
}
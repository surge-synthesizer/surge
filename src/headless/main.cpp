#include <iostream>
#include <iomanip>

#include "SurgeSynthesizer.h"

#include "HeadlessPluginLayerProxy.h"

int main(int argc, char** argv)
{
    std::cout << "Surge Headless Mode" << std::endl;

    HeadlessPluginLayerProxy *parent = new HeadlessPluginLayerProxy();
    std::unique_ptr<SurgeSynthesizer> surge(new SurgeSynthesizer(parent));
    surge->setSamplerate(44100);

    /*
    ** Change a parameter in the scene. Do this by traversing the 
    ** graph in the current patch (which is in surge->storage).
    **
    ** Clearly a more fulsome headless API would provide wrappers around
    ** this for common activities. This sets up a pair of detuned saw waves
    ** both active.
    */
    surge->storage.getPatch().scene[0].osc[0].pitch.set_value_f01(4);
    surge->storage.getPatch().scene[0].mute_o2.set_value_f01(0,true);
    surge->storage.getPatch().scene[0].osc[1].pitch.set_value_f01(1);

    /*
    ** Play a note. channel, note, velocity, detune
    */
    surge->playNote((char)0, (char)60, (char)100, 0); 

    /*
    ** Strip off some processing first to avoid the attach transient
    */
    for(auto i=0; i<20; ++i) surge->process();

    /*
    ** Then run the sampler
    */
    int blockCount = 30;
    int overSample = 8; // we want to include n samples per printed row. 
    float overS = 0;
    int sampleCount = 0;
    for (auto i = 0; i < blockCount; ++i )
    {
        surge->process();

        for (int sm = 0; sm < BLOCK_SIZE; ++sm)
        {
            float avgOut = 0;
            for (int oi = 0; oi < surge->getNumOutputs(); ++oi)
            {
                avgOut += surge->output[oi][sm];
            }

            overS += avgOut;
            sampleCount ++;

            if (((sampleCount) % overSample) == 0)
            {
                overS /= overSample;
                int gWidth = (int)((overS + 1)*30);
                std::cout << "Sample: " << std::setw( 15 ) << overS << std::setw(gWidth) << "X" << std::endl;;
                overS = 0.0; // start accumulating again
            }
        }
    }
}

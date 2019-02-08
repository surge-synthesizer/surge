#include <iostream>
#include <iomanip>

#include "HeadlessTools.h"
#include "HeadlessApps.h"

int main(int argc, char** argv)
{
    std::cout << "Surge Headless Mode" << std::endl;
    SurgeSynthesizer* surge = Surge::Headless::createSurge();

    Surge::Headless::scanAllPresets(surge);
}

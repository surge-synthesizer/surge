#include "airwindows_adapter.h"


// These are included only here

#include "ADClip7.h"
#include "Air.h"
#include "Apicolypse.h"
#include "BassDrive.h"
#include "BitGlitter.h"
#include "BlockParty.h"
#include "BrightAmbience2.h"
#include "BussColors4.h"
#include "ButterComp2.h"
#include "Cojones.h"
#include "Compresaturator.h"
#include "CrunchyGrooveWear.h"
#include "DeBess.h"
#include "DeckWrecka.h"
#include "DeEss.h"
#include "Density.h"
#include "DeRez2.h"
#include "Drive.h"
#include "DrumSlam.h"
#include "DustBunny.h"
#include "Focus.h"
#include "Fracture.h"
#include "GrooveWear.h"
#include "HardVacuum.h"
#include "Hombre.h"
#include "IronOxide5.h"
#include "Logical4.h"
#include "Loud.h"
#include "Melt.h"
#include "Mojo.h"
#include "NCSeventeen.h"
#include "Noise.h"
#include "OneCornerClip.h"
#include "PocketVerbs.h"
#include "Point.h"
#include "Pop.h"
#include "Pressure4.h"
#include "PyeWacket.h"
#include "SingleEndedTriode.h"
#include "Spiral2.h"
#include "StarChild.h"
#include "Surge.h"
#include "ToTape6.h"
#include "UnBox.h"
#include "VariMu.h"
#include "VoiceOfTheStarship.h"

void AirWindowsEffect::registerPlugins()
{
   if( ! fxreg.empty() ) return;
   
   /* 
   ** Register here with streaming ID (which must be increasing and can never change) and display order ID (which can be
   ** really anything you want) and display name. So basically always add stuff to the end of this list but set the
   ** display order to whatever you want. The nice thing is the display order doesn't need to be contiguous and the
   ** order displayed is just a sort. So here I use values in steps of 10 so I can put stuff in between.
   */

   // Set up some group names
   std::string gnDynamics = "Dynamics";
   std::string gnFilter = "Filter";
   std::string gnLoFi = "Lo-Fi";
   std::string gnNoise = "Noise";
   std::string gnAmbience = "Ambience";
   std::string gnSaturation = "Saturation";
   std::string gnTape = "Tape";

   int id=0; // add new effects only at the end of this list!

   registerAirwindow<ADClip7::ADClip7>( id++, 10, gnDynamics, "AD Clip" );
   registerAirwindow<BlockParty::BlockParty>( id++, 20, gnDynamics, "Block Party" );
   registerAirwindow<ButterComp2::ButterComp2>( id++, 30, gnDynamics, "Butter Comp" );
   registerAirwindow<Compresaturator::Compresaturator>( id++, 40, gnDynamics, "Compresaturator" );
   registerAirwindow<Logical4::Logical4>( id++, 50, gnDynamics, "Logical" );
   registerAirwindow<Mojo::Mojo>( id++, 60, gnDynamics, "Mojo" );
   registerAirwindow<OneCornerClip::OneCornerClip>( id++, 70, gnDynamics, "One Corner Clip" );
   registerAirwindow<Point::Point>( id++, 80, gnDynamics, "Point" );
   registerAirwindow<Pop::Pop>( id++, 90, gnDynamics, "Pop" );
   registerAirwindow<Pressure4::Pressure4>( id++, 100, gnDynamics, "Pressure" );
   registerAirwindow<PyeWacket::Pyewacket>( id++, 110, gnDynamics, "Pye Wacket" );
   registerAirwindow<Surge::Surge>( id++, 120, gnDynamics, "Surge" );
   registerAirwindow<VariMu::VariMu>( id++, 130, gnDynamics, "Vari-Mu" );

   registerAirwindow<BitGlitter::BitGlitter>( id++, 140, gnLoFi, "Bit Glitter" );
   registerAirwindow<CrunchyGrooveWear::CrunchyGrooveWear>( id++, 150, gnLoFi, "Crunchy Groove Wear" );
   registerAirwindow<DeRez2::DeRez2>( id++, 160, gnLoFi, "DeRez" );
   registerAirwindow<DeckWrecka::Deckwrecka>( id++, 170, gnLoFi, "Deck Wrecka" );
   registerAirwindow<DustBunny::DustBunny>(id++, 180, gnNoise, "Dust Bunny");
   registerAirwindow<GrooveWear::GrooveWear>( id++, 190, gnLoFi, "Groove Wear" );

   registerAirwindow<Noise::Noise>(id++, 200, gnNoise, "Noise");
   registerAirwindow<VoiceOfTheStarship::VoiceOfTheStarship>( id++, 210, gnNoise, "Voice Of The Starship" );

   registerAirwindow<BrightAmbience2::BrightAmbience2>( id++, 220, gnAmbience, "Bright Ambience" );
   registerAirwindow<Hombre::Hombre>(id++, 230, gnFilter, "Hombre");
   registerAirwindow<Melt::Melt>( id++, 240, gnAmbience, "Melt" );
   registerAirwindow<PocketVerbs::PocketVerbs>( id++, 250, gnAmbience, "Pocket Verbs" );
   registerAirwindow<StarChild::StarChild>( id++, 260, gnAmbience, "Star Child" );

   registerAirwindow<Apicolypse::Apicolypse>( id++, 270, gnSaturation, "Apicolypse" );
   registerAirwindow<BassDrive::BassDrive>( id++, 280, gnSaturation, "Bass Drive" );
   registerAirwindow<Cojones::Cojones>( id++, 290, gnSaturation, "Cojones" );
   registerAirwindow<Density::Density>( id++, 300, gnSaturation, "Density" );
   registerAirwindow<Drive::Drive>( id++, 310, gnSaturation, "Drive" );
   registerAirwindow<Focus::Focus>( id++, 320, gnSaturation, "Focus" );
   registerAirwindow<Fracture::Fracture>( id++, 330, gnSaturation, "Fracture" );
   registerAirwindow<HardVacuum::HardVacuum>( id++, 340, gnSaturation, "Hard Vacuum" );
   registerAirwindow<Loud::Loud>( id++, 350, gnSaturation, "Loud" );
   registerAirwindow<NCSeventeen::NCSeventeen>( id++, 360, gnSaturation, "NC-17" );
   registerAirwindow<Spiral2::Spiral2>( id++, 390, gnSaturation, "Spiral" );
   registerAirwindow<UnBox::UnBox>( id++, 400, gnSaturation, "Unbox" );

   registerAirwindow<DeBess::DeBess>(id++, 420, gnFilter, "De-Bess");
   registerAirwindow<DeEss::DeEss>(id++, 430, gnFilter, "De-Ess");

   registerAirwindow<SingleEndedTriode::SingleEndedTriode>( id++, 440, gnSaturation, "Single-Ended Triode" );

   registerAirwindow<IronOxide5::IronOxide5>( id++, 450, gnTape, "Iron Oxide" );
   registerAirwindow<ToTape6::ToTape6>( id++, 460, gnTape, "To Tape" );

   registerAirwindow<Air::Air>(id++, 400, gnFilter, "Air");

   registerAirwindow<BussColors4::BussColors4>(id++, 25, gnSaturation, "Buss Colors");
   registerAirwindow<DrumSlam::DrumSlam>( id++, 46, gnDynamics, "Drum Slam" );
}

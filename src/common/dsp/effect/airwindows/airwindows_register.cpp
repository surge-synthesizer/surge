#include "airwindows_adapter.h"


// These are included only here
#include "ADClip7.h"
#include "BlockParty.h"
#include "ButterComp2.h"
#include "Compresaturator.h"
#include "Logical4.h"
#include "Mojo.h"
#include "OneCornerClip.h"
#include "Point.h"
#include "Pop.h"
#include "Pressure4.h"
#include "PyeWacket.h"
#include "Surge.h"
#include "VariMu.h"
#include "BitGlitter.h"
#include "CrunchyGrooveWear.h"
#include "DeRez2.h"
#include "DeckWrecka.h"
#include "DustBunny.h"
#include "GrooveWear.h"
#include "Noise.h"
#include "VoiceOfTheStarship.h"
#include "BrightAmbience2.h"
#include "Hombre.h"
#include "Melt.h"
#include "PocketVerbs.h"
#include "StarChild.h"
#include "Apicolypse.h"
#include "BassDrive.h"
#include "Cojones.h"
#include "Density.h"
#include "Drive.h"
#include "Focus.h"
#include "Fracture.h"
#include "HardVacuum.h"
#include "Loud.h"
#include "NCSeventeen.h"
#include "PurestDrive.h"
#include "Spiral.h"
#include "Spiral2.h"
#include "UnBox.h"
#include "BrassRider.h"
#include "DeBess.h"
#include "DeEss.h"
#include "SingleEndedTriode.h"
#include "IronOxide5.h"
#include "ToTape6.h"


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
   std::string gnLoFi = "LoFi and Noise";
   std::string gnReverb = "Reverb and Diffusion";
   std::string gnSaturation = "Saturation";
   std::string gnSpecial = "Special";
   std::string gnTape = "Tape";

   int id=0; // Like really on add at the end to this list OK!

   registerAirwindow<ADClip7::ADClip7>( id++, 10, gnDynamics, "ADClip7" );
   registerAirwindow<BlockParty::BlockParty>( id++, 20, gnDynamics, "BlockParty" );
   registerAirwindow<ButterComp2::ButterComp2>( id++, 30, gnDynamics, "ButterComp2" );
   registerAirwindow<Compresaturator::Compresaturator>( id++, 40, gnDynamics, "Compresaturator" );
   registerAirwindow<Logical4::Logical4>( id++, 50, gnDynamics, "Logical4" );
   registerAirwindow<Mojo::Mojo>( id++, 60, gnDynamics, "Mojo" );
   registerAirwindow<OneCornerClip::OneCornerClip>( id++, 70, gnDynamics, "OneCornerClip" );
   registerAirwindow<Point::Point>( id++, 80, gnDynamics, "Point" );
   registerAirwindow<Pop::Pop>( id++, 90, gnDynamics, "Pop" );
   registerAirwindow<Pressure4::Pressure4>( id++, 100, gnDynamics, "Pressure4" );
   registerAirwindow<PyeWacket::Pyewacket>( id++, 110, gnDynamics, "PyeWacket" );
   registerAirwindow<Surge::Surge>( id++, 120, gnDynamics, "Surge" );
   registerAirwindow<VariMu::VariMu>( id++, 130, gnDynamics, "VariMu" );

   registerAirwindow<BitGlitter::BitGlitter>( id++, 140, gnLoFi, "BitGlitter" );
   registerAirwindow<CrunchyGrooveWear::CrunchyGrooveWear>( id++, 150, gnLoFi, "CrunchyGrooveWear" );
   registerAirwindow<DeRez2::DeRez2>( id++, 160, gnLoFi, "DeRez2" );
   registerAirwindow<DeckWrecka::Deckwrecka>( id++, 170, gnLoFi, "DeckWrecka" );
   registerAirwindow<DustBunny::DustBunny>( id++, 180, gnLoFi, "DustBunny" );
   registerAirwindow<GrooveWear::GrooveWear>( id++, 190, gnLoFi, "GrooveWear" );
   registerAirwindow<Noise::Noise>( id++, 200, gnLoFi, "Noise" );
   registerAirwindow<VoiceOfTheStarship::VoiceOfTheStarship>( id++, 210, gnLoFi, "VoiceOfTheStarship" );

   registerAirwindow<BrightAmbience2::BrightAmbience2>( id++, 220, gnReverb, "BrightAmbience2" );
   registerAirwindow<Hombre::Hombre>( id++, 230, gnReverb, "Hombre" );
   registerAirwindow<Melt::Melt>( id++, 240, gnReverb, "Melt" );
   registerAirwindow<PocketVerbs::PocketVerbs>( id++, 250, gnReverb, "PocketVerbs" );
   registerAirwindow<StarChild::StarChild>( id++, 260, gnReverb, "StarChild" );

   registerAirwindow<Apicolypse::Apicolypse>( id++, 270, gnSaturation, "Apicolypse" );
   registerAirwindow<BassDrive::BassDrive>( id++, 280, gnSaturation, "BassDrive" );
   registerAirwindow<Cojones::Cojones>( id++, 290, gnSaturation, "Cojones" );
   registerAirwindow<Density::Density>( id++, 300, gnSaturation, "Density" );
   registerAirwindow<Drive::Drive>( id++, 310, gnSaturation, "Drive" );
   registerAirwindow<Focus::Focus>( id++, 320, gnSaturation, "Focus" );
   registerAirwindow<Fracture::Fracture>( id++, 330, gnSaturation, "Fracture" );
   registerAirwindow<HardVacuum::HardVacuum>( id++, 340, gnSaturation, "HardVacuum" );
   registerAirwindow<Loud::Loud>( id++, 350, gnSaturation, "Loud" );
   registerAirwindow<NCSeventeen::NCSeventeen>( id++, 360, gnSaturation, "NCSeventeen" );
   registerAirwindow<PurestDrive::PurestDrive>( id++, 370, gnSaturation, "PurestDrive" );
   registerAirwindow<Spiral::Spiral>( id++, 380, gnSaturation, "Spiral" );
   registerAirwindow<Spiral2::Spiral2>( id++, 390, gnSaturation, "Spiral2" );
   registerAirwindow<UnBox::UnBox>( id++, 400, gnSaturation, "UnBox" );

   registerAirwindow<BrassRider::BrassRider>( id++, 410, gnSpecial, "BrassRider" );
   registerAirwindow<DeBess::DeBess>( id++, 420, gnSpecial, "DeBess" );
   registerAirwindow<DeEss::DeEss>( id++, 430, gnSpecial, "DeEss" );
   registerAirwindow<SingleEndedTriode::SingleEndedTriode>( id++, 440, gnSpecial, "SingleEndedTriode" );

   registerAirwindow<IronOxide5::IronOxide5>( id++, 450, gnTape, "IronOxide5" );
   registerAirwindow<ToTape6::ToTape6>( id++, 460, gnTape, "ToTape6" );
}

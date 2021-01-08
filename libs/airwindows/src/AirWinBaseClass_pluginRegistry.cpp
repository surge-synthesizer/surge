#include "ADClip7.h"
#include "Air.h"
#include "Apicolypse.h"
#include "BassDrive.h"
#include "BitGlitter.h"
#include "BlockParty.h"
#include "BrightAmbience2.h"
#include "BussColors4.h"
#include "ButterComp2.h"
#include "Capacitor.h"
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
#include "Slew.h"
#include "Slew2.h"
#include "Spiral2.h"
#include "StarChild.h"
#include "Surge.h"
#include "ToTape6.h"
#include "UnBox.h"
#include "VariMu.h"
#include "VoiceOfTheStarship.h"
#include <map>

namespace {
template<typename T>
std::unique_ptr<AirWinBaseClass> create(int id, double sr, int dp)
{
   auto res = std::make_unique<T>(id);
   res->sr = sr;
   res->displayPrecision = dp;
   return res;
}
} // anonymous namespace

std::vector<AirWinBaseClass::Registration> AirWinBaseClass::pluginRegistry()
{
   /*
    * Static function, only called from audio thread, safe to have a static member here to avoid
    * rebuild too often.
    */
   static std::vector<AirWinBaseClass::Registration> reg;

   if( ! reg.empty() )
      return reg;

   /* 
   ** Register here with streaming ID (which must be increasing and can never change) and display order ID (which can be
   ** really anything you want) and display name. So basically always add stuff to the end of this list but set the
   ** display order to whatever you want. The nice thing is the display order doesn't need to be contiguous and the
   ** order displayed is just a sort. So here I use values in steps of 10 so I can put stuff in between.
   */

   // Set up some group names
   std::string gnClipping = "Clipping";
   std::string gnDynamics = "Dynamics";
   std::string gnFilter = "Filter";
   std::string gnLoFi = "Lo-Fi";
   std::string gnNoise = "Noise";
   std::string gnAmbience = "Ambience";
   std::string gnSaturation = "Saturation And More";
   std::string gnTape = "Tape";

   int id = 0; // add new effects only at the end of this list!

   reg.emplace_back(create<ADClip7::ADClip7>, id++, 10, gnClipping, "AD Clip");
   reg.emplace_back(create<BlockParty::BlockParty>, id++, 20, gnDynamics, "Block Party");
   reg.emplace_back(create<ButterComp2::ButterComp2>, id++, 30, gnDynamics, "Butter Comp");
   reg.emplace_back(create<Compresaturator::Compresaturator>, id++, 40, gnDynamics, "Compresaturator");
   reg.emplace_back(create<Logical4::Logical4>, id++, 50, gnDynamics, "Logical");
   reg.emplace_back(create<Mojo::Mojo>, id++, 355, gnSaturation, "Mojo");
   reg.emplace_back(create<OneCornerClip::OneCornerClip>, id++, 70, gnClipping, "One Corner Clip");
   reg.emplace_back(create<Point::Point>, id++, 80, gnDynamics, "Point");
   reg.emplace_back(create<Pop::Pop>, id++, 90, gnDynamics, "Pop");
   reg.emplace_back(create<Pressure4::Pressure4>, id++, 100, gnDynamics, "Pressure");
   reg.emplace_back(create<PyeWacket::Pyewacket>, id++, 110, gnDynamics, "Pye Wacket");
   reg.emplace_back(create<Surge::Surge>, id++, 120, gnDynamics, "Surge");
   reg.emplace_back(create<VariMu::VariMu>, id++, 130, gnDynamics, "Vari-Mu");

   reg.emplace_back(create<BitGlitter::BitGlitter>, id++, 140, gnLoFi, "Bit Glitter");
   reg.emplace_back(create<CrunchyGrooveWear::CrunchyGrooveWear>, id++, 150, gnLoFi, "Crunchy Groove Wear");
   reg.emplace_back(create<DeRez2::DeRez2>, id++, 160, gnLoFi, "DeRez");
   reg.emplace_back(create<DeckWrecka::Deckwrecka>, id++, 170, gnLoFi, "Deck Wrecka");
   reg.emplace_back(create<DustBunny::DustBunny>, id++, 180, gnNoise, "Dust Bunny");
   reg.emplace_back(create<GrooveWear::GrooveWear>, id++, 190, gnLoFi, "Groove Wear");

   reg.emplace_back(create<Noise::Noise>, id++, 200, gnNoise, "Noise");
   reg.emplace_back(create<VoiceOfTheStarship::VoiceOfTheStarship>, id++, 210, gnNoise, "Voice Of The Starship");

   reg.emplace_back(create<BrightAmbience2::BrightAmbience2>, id++, 220, gnAmbience, "Bright Ambience");
   reg.emplace_back(create<Hombre::Hombre>, id++, 425, gnFilter, "Hombre");
   reg.emplace_back(create<Melt::Melt>, id++, 240, gnAmbience, "Melt");
   reg.emplace_back(create<PocketVerbs::PocketVerbs>, id++, 250, gnAmbience, "Pocket Verbs");
   reg.emplace_back(create<StarChild::StarChild>, id++, 260, gnAmbience, "Star Child");

   reg.emplace_back(create<Apicolypse::Apicolypse>, id++, 270, gnSaturation, "Apicolypse");
   reg.emplace_back(create<BassDrive::BassDrive>, id++, 280, gnSaturation, "Bass Drive");
   reg.emplace_back(create<Cojones::Cojones>, id++, 290, gnSaturation, "Cojones");
   reg.emplace_back(create<Density::Density>, id++, 300, gnSaturation, "Density");
   reg.emplace_back(create<Drive::Drive>, id++, 310, gnSaturation, "Drive");
   reg.emplace_back(create<Focus::Focus>, id++, 320, gnSaturation, "Focus");
   reg.emplace_back(create<Fracture::Fracture>, id++, 330, gnSaturation, "Fracture");
   reg.emplace_back(create<HardVacuum::HardVacuum>, id++, 340, gnSaturation, "Hard Vacuum");
   reg.emplace_back(create<Loud::Loud>, id++, 350, gnSaturation, "Loud");
   reg.emplace_back(create<NCSeventeen::NCSeventeen>, id++, 360, gnSaturation, "NC-17");
   reg.emplace_back(create<Spiral2::Spiral2>, id++, 390, gnSaturation, "Spiral");
   reg.emplace_back(create<UnBox::UnBox>, id++, 400, gnSaturation, "Unbox");

   reg.emplace_back(create<DeBess::DeBess>, id++, 420, gnFilter, "De-Bess");
   reg.emplace_back(create<AirWindowsNoOp>, id++, -1, gnFilter, "NoOp (Was: DeEss)");

   reg.emplace_back(create<SingleEndedTriode::SingleEndedTriode>, id++, 380, gnSaturation, "Single-Ended Triode");

   reg.emplace_back(create<IronOxide5::IronOxide5>, id++, 450, gnTape, "Iron Oxide");
   reg.emplace_back(create<ToTape6::ToTape6>, id++, 460, gnTape, "To Tape");

   reg.emplace_back(create<Air::Air>, id++, 400, gnFilter, "Air");

   reg.emplace_back(create<BussColors4::BussColors4>, id++, 285, gnSaturation, "Buss Colors");
   reg.emplace_back(create<DrumSlam::DrumSlam>, id++, 46, gnDynamics, "Drum Slam");

   reg.emplace_back(create<Capacitor::Capacitor>, id++, 415, gnFilter, "Capacitor");
   reg.emplace_back(create<Slew::Slew>, id++, 113, gnClipping, "Slew 1");
   reg.emplace_back(create<Slew2::Slew2>, id++, 114, gnClipping, "Slew 2");

   return reg;
}

std::vector<int> AirWinBaseClass::pluginRegistryOrdering()
{
   // See above on static
   static auto res = std::vector<int>();
   if( ! res.empty() )
      return res;

   auto r = pluginRegistry();
   auto q = std::map<std::string, std::map<int, int>>();

   for( auto const &el : r )
   {
      if( el.displayOrder >= 0 )
         q[el.groupName][el.displayOrder] = el.id;
      else
         q[el.groupName][100000000] = -1;
   }
   for( auto const &sm : q )
   {
      for( auto const &tm : sm.second )
      {
         res.push_back( tm.second );
      }
   }
   return res;
}
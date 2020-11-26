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

#include "SkinModel.h"
#include <unordered_map>
#include <map>
#include <vector>
#include <algorithm>
#include <cstring>
#include "resource.h"
#include "SkinColors.h"
#include "strnatcmp.h"

/*
 * This file implements the innards of the Connector class and the SkinColor class
 *
 * If you want to add a new connection, add it to SkinModel.h/SkinModel.cpp
 * not here.
 */


namespace Surge
{
namespace Skin
{

std::unordered_map<std::string, std::shared_ptr<Connector::Payload>> *idmap;
std::unordered_map<Connector::NonParameterConnection, std::shared_ptr<Connector::Payload>> *npcMap;
std::unordered_map<std::string, Surge::Skin::Color> *colMap;

void guaranteeMap()
{
   static bool firstTime = true;
   if( firstTime )
   {
      idmap = new std::unordered_map<std::string, std::shared_ptr<Connector::Payload>>();
      npcMap = new std::unordered_map<Connector::NonParameterConnection, std::shared_ptr<Connector::Payload>>();
      colMap = new std::unordered_map<std::string, Surge::Skin::Color>();
      firstTime = false;
   }
}

struct HarvestMaps {
   HarvestMaps() { guaranteeMap(); }
   ~HarvestMaps() {
      delete idmap;
      delete npcMap;
      delete colMap;
   }
};

HarvestMaps hmDeleter;

std::shared_ptr<Connector::Payload> makePayload( const std::string &id,
                                                float x, float y, float w, float h,
                                                Connector::Component c,
                                                Connector::NonParameterConnection n = Connector::PARAMETER_CONNECTED )
{
   guaranteeMap();
   auto res = std::make_shared<Connector::Payload>();
   res->id = id;
   res->posx = x;
   res->posy = y;
   res->w = w;
   res->h = h;
   res->defaultComponent = c;

   idmap->insert(std::make_pair(id,res));
   if( n != Connector::PARAMETER_CONNECTED )
      npcMap->insert(std::make_pair(n,res));
   return res;
}

Connector::Connector() noexcept
{
   guaranteeMap();
   payload = std::shared_ptr<Connector::Payload>();
}

Connector::Connector( const std::string &id, float x, float y ) noexcept
{
   payload = makePayload( id, x, y, -1, -1, SLIDER );
}

Connector::Connector( const std::string &id, float x, float y, Component c ) noexcept
{
   payload = makePayload( id, x, y, -1, -1, c );
}

Connector::Connector( const std::string &id, float x, float y, float w, float h, Component c ) noexcept
{
   payload = makePayload( id, x, y, w, h, c );
}

Connector::Connector( const std::string &id, float x, float y, float w, float h, Component c, NonParameterConnection n ) noexcept
{
   payload = makePayload( id, x, y, w, h, c, n );
}

Connector::Connector( const std::string &id, float x, float y, NonParameterConnection n ) noexcept
{
   payload = makePayload( id, x, y, -1, -1, NONE, n );
}

Connector & Connector::asMixerSolo() noexcept
{
   payload->defaultComponent = Connector::SWITCH;
   payload->w = 22;
   payload->h = 15;
   return withBackground(IDB_MIXER_SOLO);
}
Connector & Connector::asMixerMute() noexcept
{
   payload->defaultComponent = Connector::SWITCH;
   payload->w = 22;
   payload->h = 15;
   return withBackground(IDB_MIXER_MUTE);
}
Connector & Connector::asMixerRoute() noexcept
{
   payload->defaultComponent = Connector::HSWITCH2;
   payload->w = 22;
   payload->h = 15;
   return withHSwitch2Properties(IDB_MIXER_OSC_ROUTING, 3, 1, 3);
}

Connector & Connector::asJogPlusMinus() noexcept
{
   payload->defaultComponent = Connector::HSWITCH2;
   payload->w = 32;
   payload->h = 12;
   return withHSwitch2Properties(IDB_PREVNEXT_JOG, 2, 1, 2);
}


Connector Connector::connectorByID(const std::string &id )
{
   guaranteeMap();
   Connector c;
   if( idmap->find(id) != idmap->end() )
      c.payload = idmap->at(id);
   return c;
}
Connector Connector::connectorByNonParameterConnection(NonParameterConnection n)
{
   guaranteeMap();
   Connector c;
   if( npcMap->find(n) != npcMap->end() )
      c.payload = npcMap->at(n);
   return c;
}


std::vector<Connector> Connector::connectorsByComponentType( Connector::Component c )
{
   auto res = std::vector<Connector>();
   guaranteeMap();
   for( auto it : *idmap )
   {
      if (it.second->defaultComponent == c)
         res.push_back(Connector(it.second));
   }
   return res;
}

Color::Color( std::string name, int r, int g, int b ) : name(name), r(r), g(g), b(b), a(255) {
   guaranteeMap();
   colMap->insert( std::make_pair(name, *this ));
}
Color::Color( std::string name, int r, int g, int b, int a ) : name(name), r(r), g(g), b(b), a(a) {
   guaranteeMap();
   colMap->insert( std::make_pair(name, *this ));
}

Color Color::colorByName(const std::string &n )
{
   guaranteeMap();
   if( colMap->find(n) != colMap->end() )
      return colMap->at(n);
   return Color( n, 255, 0, 0 );
}

std::vector<Color> Color::getAllColors()
{
   guaranteeMap();
   auto res = std::vector<Color>();
   for( auto c : *colMap )
      res.push_back(c.second);
   std::sort( res.begin(), res.end(), [](const Color &a, const Color &b ) { return strcmp( a.name.c_str(), b.name.c_str() ) < 0;});
   return res;
}


std::vector<std::string> Connector::allConnectorIDs()
{
   guaranteeMap();
   auto res = std::vector<std::string>();
   for( auto c : * idmap )
      res.push_back(c.first );
   std::sort( res.begin(), res.end(), []( auto a, auto b ) {
      auto q = strnatcasecmp( a.c_str(), b.c_str() );
      return q < 0;
   } );
   return res;
}
}
}


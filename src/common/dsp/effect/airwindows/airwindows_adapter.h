// -*-c++-*-

#pragma once

#include "../Effect.h"
#include "audioeffect_airwinstub.h"

#include <vector>

class alignas(16) AirWindowsEffect : public Effect
{
public:
   AirWindowsEffect( SurgeStorage *storage, FxStorage *fxdata, pdata *pd );
   virtual ~AirWindowsEffect();

   virtual const char* get_effectname() override { return "Airwindows"; };

   virtual void init() override;
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;

   void resetCtrlTypes( bool useStreamedValues );
   
   virtual void process( float *dataL, float *dataR ) override;

   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;

   // TODO ringout and only control and suspend

   lag<float, true> param_lags[n_fx_params - 1];
   
   void setupSubFX(int awfx, bool useStreamedValues );
   std::unique_ptr<AirWinBaseClass> airwin;
   int lastSelected = -1;
   
   struct Registration {
      std::function<std::unique_ptr<AirWinBaseClass>()> generator;
      int id;
      int displayOrder;
      std::string name, groupName;
   };
   std::vector<Registration> fxreg;

   void registerPlugins();
   template<typename T> void registerAirwindow( int id, int order, std::string gn, std::string name )
   {
      Registration r;
      r.id = id; r.displayOrder = order; r.name = name; r.groupName = gn;
      r.generator = [id]() { return std::make_unique<T>(id); };
      fxreg.push_back(r);
   }

   struct AWFxSelectorMapper : public ParameterDiscreteIndexRemapper {
      AWFxSelectorMapper( AirWindowsEffect *fx ) { this->fx = fx; };

      virtual int remapStreamedIndexToDisplayIndex( int i ) override {
         return fx->fxreg[i].displayOrder;
      }
      virtual std::string nameAtStreamedIndex( int i ) override {
         return fx->fxreg[i].name;
      }
      virtual bool hasGroupNames() override { return true; }
      
      virtual std::string groupNameAtStreamedIndex( int i ) override {
         return fx->fxreg[i].groupName;
      }
      AirWindowsEffect *fx;
   };

   struct AWFxParamFormatter : public ParameterExternalFormatter {
      AWFxParamFormatter( AirWindowsEffect *fx, int i ) : fx( fx ), idx( i ) { }
      virtual void formatValue( float value, char *txt, int txtlen ) override {
         if( fx && fx->airwin )
         {
            char lab[256], dis[256];
            fx->airwin->getParameterLabel( idx, lab );
            fx->airwin->getParameterDisplay( idx, dis );
            sprintf( txt, "%s%s", dis, lab );
         }
         else
         {
            sprintf( txt, "AWA.ERROR %lf", value );
         }
      }
      AirWindowsEffect *fx;
      int idx;
   };

   std::array<std::unique_ptr<AWFxParamFormatter>, n_fx_params - 1> fxFormatters;
   
   
   std::unique_ptr<AWFxSelectorMapper> mapper;
};

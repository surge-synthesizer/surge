// -*-c++-*-

#pragma once

#include "../Effect.h"
#include "airwindows/AirWinBaseClass.h"

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

   virtual void updateAfterReload() override;

   void resetCtrlTypes( bool useStreamedValues );
   
   virtual void process( float *dataL, float *dataR ) override;

   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;

   // TODO ringout and only control and suspend
   virtual void suspend() override
   {
      hasInvalidated = true;

      if( fxdata ) {
         fxdata->p[0].deactivated = true;
         if( fxdata->p[1].ctrltype == ct_none )
         {
            setupSubFX( fxdata->p[0].val.i, true );
         }
      }
   }

   lag<float, true> param_lags[n_fx_params - 1];
   
   void setupSubFX(int awfx, bool useStreamedValues );
   std::unique_ptr<AirWinBaseClass> airwin;
   int lastSelected = -1;
   
   std::vector<AirWinBaseClass::Registration> fxreg;

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
            if( fx->fxdata->p[0].deactivated )
               fx->airwin->setParameter( idx, value );
            fx->airwin->getParameterLabel( idx, lab );
            fx->airwin->getParameterDisplay( idx, dis );
            sprintf( txt, "%s%s%s", dis, (lab[0] == 0 ? "" : " " ), lab );
         }
         else
         {
            sprintf( txt, "AWA.ERROR %lf", value );
         }
      }

      virtual bool stringToValue( const char* txt, float &outVal ) override {
         if( fx && fx->airwin )
         {
            float v;
            if( fx->airwin->parseParameterValueFromString( idx, txt, v ) )
            {
               outVal = v;
               return true;
            }
         }
         return false;
      }
      AirWindowsEffect *fx;
      int idx;
   };

   std::array<std::unique_ptr<AWFxParamFormatter>, n_fx_params - 1> fxFormatters;
   

   std::unique_ptr<AWFxSelectorMapper> mapper;
};

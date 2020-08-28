#include "airwindows_adapter.h"

AirWindowsEffect::AirWindowsEffect( SurgeStorage *storage, FxStorage *fxdata, pdata *pd ) :
   Effect( storage, fxdata, pd )
{
   for( int i=0; i<n_fx_params-1; i++ )
   {
      param_lags[i].newValue(0);
      param_lags[i].instantize();
      param_lags[i].setRate( 0.004 * BLOCK_SIZE );
   }

   mapper = std::make_unique<AWFxSelectorMapper>(this);
}

AirWindowsEffect::~AirWindowsEffect()
{
}

void AirWindowsEffect::init() {
}

const char* AirWindowsEffect::group_label(int id)
{
   switch (id)
   {
   case 0:
      return "Type";
   case 1:
   {
      if( airwin )
      {
         static char txt[1024];
         airwin->getEffectName(txt);
         return (const char*)txt;
      }
      else
      {
         return "Effect";
      }
   }
   }
   return 0;
}
int AirWindowsEffect::group_label_ypos(int id)
{
   switch (id)
   {
   case 0:
      return 1;
   case 1:
      return 5;
   }
   return 0;
}

void AirWindowsEffect::init_ctrltypes() {
   /*
   ** This looks odd right? Why not just call resetCtrlTypes?
   ** Well: When we load if we are set to ct_none then we don't
   ** stream values on. So what we do is we transiently make
   ** every 1..n_fx a ct_airwindow_param so the unstream
   ** can set values, then when we process later, resetCtrlTypes
   ** will take those prior values and assign them as new (and that's
   ** what is called if the value is changed). Also since the load
   ** will often load to a sparate instance and copy the params over
   ** we set the user_data to nullptr here to indicate that
   ** after this inti we need to do something even if the value
   ** of our FX hasn't changed.
   */
   Effect::init_ctrltypes();
   registerPlugins();

   fxdata->p[0].set_name( "FX" );
   fxdata->p[0].set_type( ct_airwindow_fx );
   fxdata->p[0].posy_offset = 1;
   fxdata->p[0].val_max.i = fxreg.size() - 1;
   fxdata->p[0].set_user_data( nullptr );

   for( int i=0; i<n_fx_params - 1; ++i )
   {
      fxdata->p[i+1].set_type( ct_airwindow_param );
      std::string w = "Airwindow " + std::to_string(i);
      fxdata->p[i+1].set_name( w.c_str() );

      if( ! fxFormatters[i] )
         fxFormatters[i] = std::make_unique<AWFxParamFormatter>(this,i);
   }

   lastSelected = -1;
}

void AirWindowsEffect::resetCtrlTypes( bool useStreamedValues ) {
   fxdata->p[0].set_name( "FX" );
   fxdata->p[0].set_type( ct_airwindow_fx );
   fxdata->p[0].posy_offset = 1;
   fxdata->p[0].val_max.i = fxreg.size() - 1;

   fxdata->p[0].set_user_data( mapper.get() );
   
   if( airwin )
   {
      for( int i=0; i<airwin->paramCount && i < n_fx_params - 1; ++i )
      {
         char txt[1024];
         airwin->getParameterName( i, txt );
         auto priorVal = fxdata->p[i+1].val.f;
         fxdata->p[i+1].set_name( txt );
         if( airwin->isParameterBipolar( i ) )
         {
            fxdata->p[i+1].set_type( ct_airwindow_param_bipolar );
         }
         else
         {
            fxdata->p[i+1].set_type( ct_airwindow_param );
         }
         fxdata->p[i+1].set_user_data( fxFormatters[i].get() );
         fxdata->p[i+1].posy_offset = 3;

         if( useStreamedValues )
            fxdata->p[i+1].val.f = priorVal;
         else
            fxdata->p[i+1].val.f = airwin->getParameter( i );
      }
      for( int i=airwin->paramCount; i < n_fx_params; ++i )
      {
         fxdata->p[i+1].set_type( ct_none );
      }
   }

   hasInvalidated = true;
}

void AirWindowsEffect::init_default_values() {
   fxdata->p[0].val.i = 0;
   for( int i=0; i<10; ++i )
      fxdata->p[i+1].val.f = 0;
}

void AirWindowsEffect::process( float *dataL, float *dataR )
{
   if( !airwin || fxdata->p[0].val.i != lastSelected || fxdata->p[0].user_data == nullptr )
   {
      /*
      ** So do we want to let airwindows set params as defaults or do we want 
      ** to use the values on our params if we recreate? Well we have two cases.
      ** If the userdata on p0 is null it means we have unstreamed somethign but
      ** we have not set up airwindows. So this means we are loading an FXP, 
      ** a config xml snapshot, or similar.
      **
      ** If the userdata is set up and the last selected is changed then that
      ** means we have used a UI or automation gesture to re-modify a current
      ** running airwindow, so apply the defaults
      */
      bool useStreamedValues = false; 
      if( fxdata->p[0].user_data == nullptr )
      {
         useStreamedValues = true;
      }
      setupSubFX( fxdata->p[0].val.i, useStreamedValues );
   }

   if( ! airwin ) return;
   
   for( int i=0; i<airwin->paramCount && i < n_fx_params - 1; ++i )
   {
      param_lags[i].newValue( limit_range( *f[i+1], 0.f, 1.f ) );
      airwin->setParameter( i, param_lags[i].v );
      param_lags[i].process();
   }

   float outL alignas(16)[BLOCK_SIZE], outR alignas(16)[BLOCK_SIZE];
   float* in[2];
   in[0] = dataL;
   in[1] = dataR;

   float *out[2];
   out[0] = &( outL[ 0 ] );
   out[1] = &( outR[ 0 ] );

   airwin->processReplacing( in, out, BLOCK_SIZE );

   copy_block( outL, dataL, BLOCK_SIZE_QUAD );
   copy_block( outR, dataR, BLOCK_SIZE_QUAD );
}

void AirWindowsEffect::setupSubFX( int sfx, bool useStreamedValues )
{
   Registration r = fxreg[sfx];
   airwin = r.generator();
   
   char fxname[1024];
   airwin->getEffectName(fxname);
   lastSelected = sfx;
   resetCtrlTypes( useStreamedValues );
}


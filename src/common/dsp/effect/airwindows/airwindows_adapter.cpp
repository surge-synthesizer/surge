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
   registerPlugins();
   Effect::init_ctrltypes();
   fxdata->p[0].set_name( "FX Selector" );
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
         fxdata->p[i+1].set_name( txt );
         fxdata->p[i+1].set_type( ct_airwindow_param );
         fxdata->p[i+1].posy_offset = 3;
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
   if( fxdata->p[0].val.i != lastSelected )
   {
      setupSubFX( fxdata->p[0].val.i );
   }

   if( ! airwin ) return;
   
   for( int i=0; i<airwin->paramCount && i < n_fx_params - 1; ++i )
   {
      param_lags[i].newValue( *f[i+1] );
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

void AirWindowsEffect::setupSubFX( int sfx )
{
   Registration r = fxreg[sfx];
   airwin = r.generator();
   
   char fxname[1024];
   airwin->getEffectName(fxname);
   std::cout << "Constructed " << fxname << " " << airwin->paramCount << std::endl;
   lastSelected = sfx;
   init_ctrltypes();
}


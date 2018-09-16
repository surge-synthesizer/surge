#include "surgecontroller.h"

#include "pluginterfaces/base/ustring.h"

tresult PLUGIN_API SurgeController::initialize (FUnknown* context)
{
   tresult result = EditControllerEx1::initialize (context);
   if (result != kResultOk)
   {
      return result;
   }

   //--- Create Units-------------
   UnitInfo unitInfo;
   Unit* unit;

   // create root only if you want to use the programListId
   /*	unitInfo.id = kRootUnitId;	// always for Root Unit
   	unitInfo.parentUnitId = kNoParentUnitId;	// always for Root Unit
   	Steinberg::UString (unitInfo.name, USTRINGSIZE (unitInfo.name)).assign (USTRING ("Root"));
   	unitInfo.programListId = kNoProgramListId;

   	unit = new Unit (unitInfo);
   	addUnitInfo (unit);*/

   // create a unit1
   unitInfo.id = 1;
   unitInfo.parentUnitId = kRootUnitId;	// attached to the root unit

   Steinberg::UString (unitInfo.name, USTRINGSIZE (unitInfo.name)).assign (USTRING ("Unit1"));

   unitInfo.programListId = kNoProgramListId;

   unit = new Unit (unitInfo);
   addUnit (unit);

   //---Create Parameters------------



   /*GainParameter* gainParam = new GainParameter (ParameterInfo::kCanAutomate, 0);
   parameters.addParameter (gainParam);*/


   return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SurgeController::terminate  ()
{
   return EditControllerEx1::terminate ();
}
// -*-c++--*-
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

/*
** This file also includes the 2020 non-fiction work, "A Short Essay on Indices in
** Surge".
**
** A Short Essay on Indices in Surge
**
** Indices in Surge are complicated, and are complicated for a couple of reasons
**
** 1. The SurgePatch object has a list of parameters which have indices which are
**    handed out linearly by their declaration in SurgePatch.
** 2. The SurgeStorage object indicates a layout of types and subtypes, but the
**    code is full of implicit and explicit assumptions that the parameter indices
**    of those parameters are adjacent. That is, the parameters in OSCStorage
**    are a contiguious non-overlapping bunch of parameter IDs. Moreover, SurgeStorage
**    assumes everywhere that given a parameter with ID, that param_ptr[p->id] is that
**    parameter, so the internal IDs are also the array indices.
** 3. The DAWs expose a variety of phantom IDs. For instance, the Macros are
**    the first 8 DAW parameters, but they are not actually parameters at all
**    in Surge.
** 4. SurgeGUIEditor uses the VSTGUI concept of 'tag' on a control, but some controls
**    also don't bind to parameters (like the Menu button) so it has a collection
**    of control tags which are 'up front' in tag space. That means a SurgeGUIEditor
**    tag from control->getTag() will either be internal (if c->getTag() < start_paramtags)
**    or refer to an internal parameter index (control->getTag() - start_paramtags is an index
**    into the synth->param_ptr array).
** 5. The VST3 makes it worse since, as VST3 doesn't support MIDI control inputs, we
**    have had to create MIDI control port virtual parameters after the end of Surge
**    params, so the VST3 is full of ifs which work but are not very well documented.
**
** Luckily, patch streaming is immune to all of this since patches (and LV2 control ports)
** use the streaming name. So this problem only really rears its hed if those DAW phantom
** IDs align with the parameter IDs as 1:1 offsets. If that was the case, then adding
** a parameter would change VST IDs. And that is the case in Surge 1.7.1 and earlier.
**
** So what does it mean to add a parameter? Well let's consider adding a parameter to
** OSCStorage so oscillators had 8, not 7, slots each. If you just did that willy nilly
** today here's what would happen:
**
** 1. You would change n_osc_params from 7 to 8 and recompile. Great. Everything would work
**    in headless and the synth would come up no problem and load patches and run. If we had
**    some place put in a '7' rather than an 'n_osc_params' you would have to find and fix that.
** 2. The VST2/3/AU ID of every item after scene 1 / osc 1 would increase by 3 (in scene A)
**    and 6 (in scene B)
** 3. So almost all your old automation would map to the wrong spot in your DAW.
**
** So this file exists to "not have that happen" (or more accurately "make it so that cannot
 * happen when we expand"). How does it work?
 *
 * Well first, SurgeSynthesizer has had all of its internal APIs to do things like get and set
 * parameters converted from int as index to an ID object as index. This solves one of the biggest
 * confusing points, which is "is the int I am taking about the param index, the GUI index, the DAW
 * index, or the DAW ID". Once that's done you get obvious constructors for those, which are the
 * from methods. (Note the fromGUITag is a static on SurgeGUIEditor since that requires GUI information
 * but also is only called from GUI-aware clients).
 *
 * And then you have two strategies.
 *
 * For hosts which match the DAW index and DAW ID (! PLUGIN_ID_AND_INDEX_ARE_DISTINCT, which in
 * this implementation are the VST2, LV2 and AU) you build everything on the DAWSideIndex and
 * the SynthID and basically convert back and forth using get methods at control points.
 *
 * For the VST3 which can issue IDs for its parameters non-monotonically, we have the DAWSideIndex
 * and the DAWSideID as distinct values. This means that when we add an item in 'order' we can
 * keep its ID as whatever we want and not break stremaing in the future.
 *
 * And then these classes implement the 1.8-style ID management which is
 *
 * (INDEX version)
 *    DAW params 0->7 map to the custom controls, which have SynthID metaparam_offset_i
 *    DAW params 8->n_params map to the params, which have identical SynthID values
 *    DAW params n_params -> n_params + 7 map to the first 8 params, displaces by the controls, which have
 *         SynthID 0-7.
 *
 *  The ID version basically keeps the DAW ID and the SynthID the same for now. In a future version
 *  where we expand SynthIDs (which remember will need to be continugous) that constraint will break
 *  to preserve streaming.
*/

#include "SurgeSynthesizer.h"
#include "DebugHelpers.h"

bool SurgeSynthesizer::fromDAWSideIndex( int i, ID &q ) {
   q.dawindex = i;
   if( i < num_metaparameters )
   {
      q.synthid = i + metaparam_offset;
#if PLUGIN_ID_AND_INDEX_ARE_DISTINCT
      q.dawid = i + metaparam_offset;
#endif
      return true;
   }
   else if( i < n_total_params )
   {
      q.synthid = i;
#if PLUGIN_ID_AND_INDEX_ARE_DISTINCT
      q.dawid = i;
#endif
      return true;
   }
   else if( i >= n_total_params && i < n_total_params + num_metaparameters)
   {
      q.synthid = i - n_total_params;
#if PLUGIN_ID_AND_INDEX_ARE_DISTINCT
      q.dawid = i - n_total_params;
#endif
      return true;
   }
   return false;
}

#if PLUGIN_ID_AND_INDEX_ARE_DISTINCT
bool SurgeSynthesizer::fromDAWSideId( int i, ID &q ) {
   q.dawid = i;
   q.synthid = i;

   if( i >= metaparam_offset )
   {
      q.dawindex = i - metaparam_offset;
   }
   else if( i < num_metaparameters )
   {
      q.dawindex = i + n_total_params;
   }
   else
   {
      q.dawindex = i;
   }
   
   // TODO - generate the index
   return true;
}
#endif

bool SurgeSynthesizer::fromSynthSideId( int i, ID &q ) {
#if PLUGIN_ID_AND_INDEX_ARE_DISTINCT
   q.dawid = i;
#endif
   q.synthid = i;

   if( i >= metaparam_offset )
   {
      q.dawindex = i - metaparam_offset;
   }
   else if( i < num_metaparameters )
   {
      q.dawindex = i + n_total_params;
   }
   else
   {
      q.dawindex = i;
   }
   return true;
}


bool SurgeSynthesizer::fromSynthSideIdWithGuiOffset(int i,
                                                    int start_paramtags,
                                                    int start_metacontrol_tag,
                                                    ID& q)
{
   bool res = false;
   if( i >= start_paramtags  && i <= start_paramtags + n_total_params )
      res = fromSynthSideId(i-start_paramtags, q ); // wrong for macros and stuff
   else if( i >= start_metacontrol_tag && i <= start_metacontrol_tag + num_metaparameters )
      res = fromSynthSideId(i-start_metacontrol_tag+metaparam_offset, q );
   return res;
}
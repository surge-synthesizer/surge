// -*-c++-*-
/*
** Given a juce::MidiMessage, adapt it to a SurgeSynthesizer. If we write multiple
** plugs we will need this multiple times.
*/

#pragma once

namespace Surge
{
   namespace JuceShared
   {
      template< class M, class S > // really intended to be instantiated with juce::MidiMessage and SurgeSynthesizer
      inline void messageToSynth( const M& msg, S* surge )
      {
         if( msg.isNoteOn() )
         {
            surge->playNote( msg.getChannel(), msg.getNoteNumber(), msg.getVelocity(), 0 );
         }
         else if( msg.isNoteOff() )
         {
            surge->releaseNote( msg.getChannel(), msg.getNoteNumber(), msg.getVelocity() );
         }
         else if( msg.isPitchWheel() )
         {
            surge->pitchBend( msg.getChannel(), msg.getPitchWheelValue() - 8192 );
         }
         else if( msg.isController() )
         {
            surge->channelController( msg.getChannel(), msg.getControllerNumber(), msg.getControllerValue() );
         }
         else if( msg.isChannelPressure() )
         {
            surge->channelAftertouch( msg.getChannel(), msg.getChannelPressureValue() );
         }
         else if( msg.isAllNotesOff() )
         {
            surge->allNotesOff();
         }
         else if( msg.isAllSoundOff()) {
            // Fix this - surge doesn't have a sound off message
            surge->allNotesOff();
         }
      }

      template< class P, class S >
      inline void playheadToTimeData( P* ph, S *surge, int sampleCount )
      {
         auto *td = &(surge->time_data);
         if( ph )
         {
            typename P::CurrentPositionInfo cpi;
            ph->getCurrentPosition( cpi );
            td->tempo = cpi.bpm;
            td->timeSigNumerator = cpi.timeSigNumerator;
            td->timeSigDenominator = cpi.timeSigDenominator;
         }
         else
         {
            td->tempo = 120;
            td->timeSigNumerator = 4;
            td->timeSigDenominator = 4;
         }

         td->ppqPos += sampleCount * td->tempo * dsamplerate_inv / 60.f;
      }
   }
}

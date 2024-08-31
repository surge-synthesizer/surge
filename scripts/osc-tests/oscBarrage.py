
from osc4py3.as_eventloop import *
from osc4py3 import oscbuildparse
from random import random
from threading import Timer

ip = "127.0.0.1"
port = 53280	#Surge XT default OSC in port

just_rat = [
	1./1.,
	2./1.,
	3./2.,
	4./3.,
	5./4.,
	8./5.,
	6./5.,
	5./3.,
	9./8.,
	7./4.,
	7./6.,
	8./7.,
	10./9.,
	9./5.,
	10./7.,
	7./5.,
	25./24.,
	17./16.
]

timer_period = .010   # 10 millisecond timer
periods = [1, 2]

note_dur = 1.05  # as factor of slice time

cur_period = periods[int(random() * len(periods))]
id = 1
lowfreq = 100.
hifreq = 600.
chord = []
chordsize = 9
former_tonic = 600.
new_tonic = 0.
ticks = 1
to_be_released = []
releases = []


class RepeatTimer(Timer):
    def run(self):
        while not self.finished.wait(self.interval):
            self.function(*self.args, **self.kwargs)

def nextTick():
    global ticks, to_be_released, cur_period, timer_period, id
    handle_releases()
    ticks -= 1
    if (ticks <= 0):
        if (random() < .2):
            cur_period = periods[int(random() * len(periods))]
        ticks = cur_period

        #chooseChord()
        chooseMIDIChord()
        relid = id
        #freqNotesBundled(chord, 90, id)
        midiNotesBundled(chord, 120, id)
        id += len(chord)

        for intv in chord:
            dur_ticks = int(cur_period * note_dur)
            to_be_released.append([dur_ticks, relid])
            relid += 1

def handle_releases():
    global to_be_released, releases
    releases.clear()
    for i in range(len(to_be_released) - 1, -1, -1):
        to_be_released[i][0] -= 1
        if (to_be_released[i][0] == 0):
            releases.append(to_be_released[i][1])
            to_be_released.pop(i)
    if (len(releases)):
        #freqNoteReleasesBundled(127, releases)
        midiNoteReleasesBundled(127, releases)


def freqNote(freq, vel, noteid):
    msg = oscbuildparse.OSCMessage("/fnote", ",fff", [freq, vel, noteid])
    osc_send(msg, "oscout")
    osc_process()

def freqNotesBundled(freqs, vel, noteid):
    msgs = [oscbuildparse.OSCMessage("/fnote", ",fff", [freq, vel, noteid + i])
            for i, freq in enumerate(freqs)]
    osc_send(oscbuildparse.OSCBundle(oscbuildparse.OSC_IMMEDIATELY, msgs), "oscout")
    osc_process()

def freqNoteReleasesBundled(vel, noteids):
    msgs = [oscbuildparse.OSCMessage("/fnote/rel", ",fff", [-1, vel, nid])
           for i, nid in enumerate(noteids)]
    osc_send(oscbuildparse.OSCBundle(oscbuildparse.OSC_IMMEDIATELY, msgs), "oscout")
    osc_process()

def midiNotesBundled(notes, vel, noteid):
    msgs = [oscbuildparse.OSCMessage("/mnote", ",fff", [note, vel, noteid + i])
            for i, note in enumerate(notes)]
    osc_send(oscbuildparse.OSCBundle(oscbuildparse.OSC_IMMEDIATELY, msgs), "oscout")
    osc_process()

def midiNoteReleasesBundled(vel, noteids):
    msgs = [oscbuildparse.OSCMessage("/mnote/rel", ",fff", [-1, vel, nid])
           for i, nid in enumerate(noteids)]
    osc_send(oscbuildparse.OSCBundle(oscbuildparse.OSC_IMMEDIATELY, msgs), "oscout")
    osc_process()

def allNotesOff():
    msg = oscbuildparse.OSCMessage("/allnotesoff", None, [])
    osc_send(msg, "oscout")
    osc_process()

def chooseChord():
    global former_tonic, new_tonic
    chord.clear()
    tonic_int = just_rat[int(random() * 8)]
    if (former_tonic > 1100):
        tonic_int = 1 / tonic_int
    else:
        if (former_tonic > 200) and (random() < .5):
            tonic_int = 1 / tonic_int
    
    new_tonic = former_tonic * tonic_int
    former_tonic = new_tonic
    chord.append(new_tonic)
    for i in range(0, chordsize-1):
        chord.append(chord[0] * just_rat[int(random() * 9)])
    
def chooseMIDIChord():
    global former_tonic, new_tonic
    chord.clear()
    tonic = random() * 90
    chord.append(tonic)
    for i in range(0, chordsize-1):
        chord.append(tonic + random() * 12)
    


osc_startup()
osc_udp_client(ip, port, "oscout")

tick = 0
timer = RepeatTimer(timer_period, nextTick)
timer.start()

try:
    while True:
        timer.join
except KeyboardInterrupt:
    timer.cancel()
    for r in to_be_released:
        releases.append(r[1])
        freqNoteReleasesBundled(127, releases)
    osc_terminate()
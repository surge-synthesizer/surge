from osc4py3.as_eventloop import *
from osc4py3 import oscbuildparse as obp
from osc4py3.oscmethod import *
import time
from threading import Thread

ip = "127.0.0.1"
surgeOSCInPort = 53280	#Surge XT default OSC in port
surgeOSCOutPort = 53281	#Surge XT default OSC out port

def freqNote(freq, vel, noteid):
    msg = obp.OSCMessage("/fnote", ",fff", [freq, vel, noteid])
    osc_send(msg, "oscout")
    osc_process()

def freqNoteRelease(freq, vel, noteid):
    msg = obp.OSCMessage("/fnote/rel", ",fff", [freq, vel, noteid])
    osc_send(msg, "oscout")
    osc_process()

def midiNote(freq, vel, noteid):
    msg = obp.OSCMessage("/mnote", ",fff", [freq, vel, noteid])
    osc_send(msg, "oscout")
    osc_process()

def midiNoteRelease(freq, vel, noteid):
    msg = obp.OSCMessage("/mnote/rel", ",fff", [freq, vel, noteid])
    osc_send(msg, "oscout")
    osc_process()

def notesTest():
    freqNote(218.022, 100, 1)
    freqNote(261.626, 100, 2)
    freqNote(348.834, 100, 3)

    time.sleep(3)

    freqNoteRelease(0, 127, 1)
    freqNoteRelease(0, 127, 2)
    freqNoteRelease(0, 127, 3)

    time.sleep(2)

    midiNote(60, 100, 1)
    midiNote(65, 100, 2)
    midiNote(72, 100, 3)

    time.sleep(3)

    midiNoteRelease(0, 127, 1)
    midiNoteRelease(0, 127, 2)
    midiNoteRelease(0, 127, 3) 

def handlerfunction(*args):
    """for arg in args:
        print(arg + " ", end="")
    print()"""

def endless_while():
    global thread_running
    while thread_running:
        osc_process()
        time.sleep(0.1)

def get_input():
    user_input = input('Listening for OSC messages from Surge (hit any key, and return, to quit) \n')
    print('Exiting...')


osc_startup()
osc_udp_client(ip, surgeOSCInPort, "oscout")
osc_udp_server(ip, surgeOSCOutPort, "oscin")
osc_method("/*/*", handlerfunction, argscheme=OSCARG_ADDRESS + OSCARG_DATAUNPACK)

def trigger_crash():
    msg = obp.OSCMessage("/param/a/osc/1/type", ",f", [9.0])
    osc_send(msg, "oscout")
    osc_process()
    
    msg = obp.OSCMessage("/param/a/osc/1/type", ",f", [10.0])
    osc_send(msg, "oscout")
    osc_process()
    
    msg = obp.OSCMessage("/q/all_params", ",", [])
    osc_send(msg, "oscout")
    osc_process()

notesTest()
trigger_crash()
thread_running = True

if __name__ == '__main__':
    t1 = Thread(target=endless_while)
    t2 = Thread(target=get_input)

    t1.start()
    t2.start()

    t2.join()
    thread_running = False
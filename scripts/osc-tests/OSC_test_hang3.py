from osc4py3.as_eventloop import *
from osc4py3 import oscbuildparse as obp
from osc4py3.oscmethod import *
import time
from threading import Thread


localhost = "127.0.0.1"
surgeOSCInPort = 53280	#Surge XT default OSC in port
surgeOSCOutPort = 53281	#Surge XT default OSC out port

def loadPatch(patchname):
    msg = obp.OSCMessage("/patch/load", ",s", [patchname])
    osc_send(msg, "oscout")
    osc_process()

def freqNote(freq, vel):
    msg = obp.OSCMessage("/fnote", ",ff", [freq, vel])
    osc_send(msg, "oscout")
    osc_process()

def freqNoteRelease(freq, vel):
    msg = obp.OSCMessage("/fnote/rel", ",ff", [freq, vel])
    osc_send(msg, "oscout")
    osc_process()

def allNotesOff():
    msg = obp.OSCMessage("/allnotesoff", None, [])
    osc_send(msg, "oscout")
    osc_process()    

def allSoundOff():
    msg = obp.OSCMessage("/allsoundoff", None, [])
    osc_send(msg, "oscout")
    osc_process()    

def hanger():
    vel = 60
    freqNote(200, vel)
    freqNote(300, vel)
    freqNote(400, vel)
    freqNote(500, vel)
    time.sleep(2)
    allNotesOff()
    time.sleep(1)
    allNotesOff()
    time.sleep(1)
    allNotesOff()
    time.sleep(1)
    allNotesOff()


def handlerfunction(*args):
    """for arg in args:
        print(arg + " ", end="")
    print()"""

def get_input():
    user_input = input('(hit return to quit) \n')
    allSoundOff()
    print('Exiting...')


osc_startup()
osc_udp_client(localhost, surgeOSCInPort, "oscout")
osc_udp_server(localhost, surgeOSCOutPort, "oscin")
osc_method("/*/*", handlerfunction, argscheme=OSCARG_ADDRESS + OSCARG_DATAUNPACK)

#loadPatch("/Library/Application Support/Surge XT/patches_factory/Basses/Stone")
thread_running = True
hanger()
 
if __name__ == '__main__':
    t1 = Thread(target=get_input)
    t1.start()

    t1.join()
    thread_running = False

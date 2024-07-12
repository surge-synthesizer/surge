from osc4py3.as_eventloop import *
from osc4py3 import oscbuildparse as obp
from osc4py3.oscmethod import *
import time
from threading import Thread

ip = "127.0.0.1"
surgeOSCInPort = 53280	#Surge XT default OSC in port
surgeOSCOutPort = 53281	#Surge XT default OSC out port

def setscl():
    print("Sending SCL Message");
    msg = obp.OSCMessage("/tuning/scl", ",s", ["/Users/paul/dev/music/surge/resources/test-data/scl/31edo"])
    osc_send(msg, "oscout")
    osc_process()

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

setscl()
thread_running = True

if __name__ == '__main__':
    t1 = Thread(target=endless_while)
    t2 = Thread(target=get_input)

    t1.start()
    t2.start()

    t2.join()
    thread_running = False

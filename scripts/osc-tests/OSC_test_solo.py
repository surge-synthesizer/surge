from osc4py3.as_eventloop import *
from osc4py3 import oscbuildparse as obp
from osc4py3.oscmethod import *
import time

def set_solo(val):
    print("Setting solo to ", val)
    msg = obp.OSCMessage("/param/a/mixer/osc1/solo", ",f", [val])
    osc_send(msg, "oscout")
    osc_process()

### config
ip = "127.0.0.1"
surgeOSCInPort = 53280	#Surge XT default OSC in port

osc_startup()
osc_udp_client(ip, surgeOSCInPort, "oscout")

for i in range(40):
    set_solo(False)
    time.sleep(2)
    set_solo(True)
    time.sleep(2)

set_solo(False)


osc_terminate()
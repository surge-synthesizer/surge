# brew install python-tk is the trick you want
import tkinter as tk
from osc4py3.as_eventloop import *
from osc4py3 import oscbuildparse
# from tkinter import messagebox

# Function to handle button clicks
def button_click(button_id):
    if button_id == 1:
        # messagebox.showinfo("Button 1", "You clicked Button 1!")
        msg = oscbuildparse.OSCMessage("/fx/param/1", ",f", [0.2])
        osc_send(msg, "oscout")
        osc_process()

    elif button_id == 2:
        # messagebox.showinfo("Button 2", "You clicked Button 2!")
        msg = oscbuildparse.OSCMessage("/fx/param/1", ",f", [0.7])
        osc_send(msg, "oscout")
        osc_process()


# Function to create buttons
def create_buttons():
    button1 = tk.Button(root, text="Send '0'", command=lambda: button_click(1))
    button1.pack()

    button2 = tk.Button(root, text="Send '1'", command=lambda: button_click(2))
    button2.pack()


# Create the main window
root = tk.Tk()
root.title("Simple OSC Test")
root.minsize(200, 100)

# init OSC
ip = "127.0.0.1"
port = 53290        #Surge FX default OSC in port
osc_startup()
osc_udp_client(ip, port, "oscout")


# Call function to create buttons
create_buttons()

# Start the tkinter main loop
root.mainloop()

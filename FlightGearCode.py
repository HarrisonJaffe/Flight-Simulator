"""
Simple telnet example that makes the altitude increase.
"""

import time
from pprint import pprint
from flightgear_python.fg_if import TelnetConnection

"""
Start FlightGear with `"C:\Program Files\FlightGear 2024.1\bin\fgfs.exe" --telnet=socket,bi,60,localhost,5500,tcp`
"""

telnet_conn = TelnetConnection('localhost', 5500)
telnet_conn.connect()  # Make an actual connection
telnet_props = telnet_conn.list_props('/', recurse_limit=0)
pprint(telnet_props)  # List the top-level properties, no recursion

def sim_data():
    alt_ft = telnet_conn.get_prop('/position/altitude-ft')
    pitch = telnet_conn.get_prop('/orientation/pitch-deg')
    roll = telnet_conn.get_prop('/orientation/roll-deg')
    #print(f'Altitude: {alt_ft:.1f}ft')
    #print(f'Pitch: {pitch:.1f}deg')
    #print(f'Roll: {roll:.1f}deg')
    '''alt_ft = 100
    pitch = 20.2
    roll = 10
    print("check")'''
    return alt_ft, pitch, roll

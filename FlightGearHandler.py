"""
Start FlightGear with `"C:\Program Files\FlightGear 2024.1\bin\fgfs.exe" --telnet=socket,bi,60,localhost,5500,tcp`
 or with "C:\Program Files\FlightGear 2024.1\bin\fgfs.exe" --telnet=socket,bi,60,localhost,5500,tcp --aircraft=777-300
"""

import serial
import time
from FlightGearCode import sim_data

print("FlightGear data test:", sim_data())

# Global variables for connection management
arduino = None
connection_attempts = 0
max_connection_attempts = 5

def connect_to_arduino():
    """Establish connection to Arduino with retry logic"""
    global arduino, connection_attempts
    
    try:
        if arduino and arduino.is_open:
            arduino.close()
            time.sleep(1)
        
        arduino = serial.Serial(port='COM5', baudrate=115200, timeout=0.1)  # SHORT timeout
        print(f"Connected to {arduino.name}")
        
        # Clear buffers
        arduino.reset_input_buffer()
        arduino.reset_output_buffer()
        time.sleep(2)  # Give Arduino time to initialize
        
        connection_attempts = 0
        return True
        
    except serial.SerialException as e:
        print(f"Failed to connect to Arduino: {e}")
        connection_attempts += 1
        return False

def check_arduino_connection():
    """Check if Arduino connection is still alive"""
    global arduino
    
    if not arduino or not arduino.is_open:
        return False
    
    try:
        # Try to access port properties - will fail if disconnected
        arduino.in_waiting  # This will throw exception if disconnected
        return True
    except:
        return False

def send_data_simple(pitch, roll):
    """Simplified data sending - no complex retry logic"""
    global arduino
    
    try:
        # Check connection first
        if not check_arduino_connection():
            print("Arduino connection lost")
            return False
        
        data = f"{pitch:.2f},{roll:.2f}\n"
        arduino.write(data.encode('utf-8'))
        arduino.flush()
        return True
        
    except Exception as e:
        print(f"Send error: {e}")
        return False

def read_arduino_simple():
    """Simplified Arduino reading - non-blocking"""
    global arduino
    
    try:
        if not check_arduino_connection():
            return None
        
        # Only read if data is available
        if arduino.in_waiting > 0:
            # Read available bytes with short timeout
            try:
                data = arduino.read(arduino.in_waiting)
                if data:
                    text = data.decode('utf-8', errors='ignore').strip()
                    if text and text not in ["ACK"]:  # Filter out ACK spam
                        return text
            except:
                # If decode fails, clear buffer and continue
                arduino.reset_input_buffer()
                return None
        
        return None
        
    except Exception as e:
        print(f"Read error: {e}")
        return None

def wait_for_arduino_ready(timeout=10):
    """Wait for Arduino ready signal"""
    print("Waiting for Arduino to be ready...")
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        if not check_arduino_connection():
            if not connect_to_arduino():
                time.sleep(1)
                continue
        
        # Check for ready signal
        response = read_arduino_simple()
        if response:
            print(f"Arduino: {response}")
            if "READY" in response:
                print("Arduino is ready!")
                return True
        
        time.sleep(0.2)
    
    print("Arduino ready timeout - continuing anyway")
    return False

# Initial connection
print("Connecting to Arduino...")
if not connect_to_arduino():
    print("Could not establish initial connection. Exiting...")
    exit(1)

# Wait for Arduino to be ready
wait_for_arduino_ready()

# Main data loop - SIMPLIFIED
print("Starting main data loop...")
print("=" * 50)

loop_count = 0
last_successful_send = time.time()
send_interval = 0.1  # Send every 100ms
last_print_time = time.time()
print_interval = 2.0  # Print status every 2 seconds

try:
    while True:
        current_time = time.time()
        
        # Send data at regular intervals
        if current_time - last_successful_send >= send_interval:
            try:
                # Get flight data
                alt_ft, pitch, roll = sim_data()
                
                # Send data
                if send_data_simple(pitch, roll):
                    last_successful_send = current_time
                    loop_count += 1
                else:
                    print("Send failed - attempting reconnect...")
                    if connect_to_arduino():
                        print("Reconnected successfully")
                    else:
                        print("Reconnect failed")
                
            except Exception as e:
                print(f"Error getting flight data: {e}")
                time.sleep(0.5)
        
        # Read any Arduino responses (non-blocking)
        try:
            response = read_arduino_simple()
            if response:
                # Only print important messages
                if "Target:" in response or "Error:" in response or "Platform" in response:
                    print(f"Arduino: {response}")
        except Exception as e:
            print(f"Error reading Arduino: {e}")
        
        # Print status periodically
        if current_time - last_print_time >= print_interval:
            print(f"Status: Loop {loop_count}, Connection: {'OK' if check_arduino_connection() else 'LOST'}")
            last_print_time = current_time
        
        # Small delay to prevent CPU overload
        time.sleep(0.01)  # 10ms delay
            
except KeyboardInterrupt:
    print("\nStopping data stream...")
except Exception as e:
    print(f"Unexpected error: {e}")
finally:
    if arduino and arduino.is_open:
        try:
            arduino.close()
        except:
            pass
    print("Serial connection closed")
    print("=" * 50)
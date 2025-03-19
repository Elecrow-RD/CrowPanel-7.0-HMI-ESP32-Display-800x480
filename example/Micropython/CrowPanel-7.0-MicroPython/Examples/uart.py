import machine
import time

# Initialize UART
uart = machine.UART(1, baudrate=115200, tx=43, rx=44)

def send_data(data):
    uart.write(data)  # Send data via UART
    print("Sent:", data)

def receive_data():
    if uart.any():  # Check for readable data
        data = uart.read()  # retrieve data
        print("Received:", data)
        return data
    return None

# Example: Sending and Receiving Data
send_data('Hello, UART!\n')

while True:
    received = receive_data()
    if received:
        # do something about it
        pass
    time.sleep(1)


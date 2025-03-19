import machine
import os
import sdcard
import uos

#  SD Card Initialization
def init_sd():
    # Creating SPI Objects
    spi = machine.SPI(2, baudrate=1000000, polarity=0, phase=0, sck=machine.Pin(12), mosi=machine.Pin(11), miso=machine.Pin(13))
    cs = machine.Pin(10, machine.Pin.OUT)

    # SD Card Initialization
    sd = sdcard.SDCard(spi, cs)
    vfs = uos.VfsFat(sd)
    uos.mount(vfs, "/sd")

    print("SD card initialization complete")
    print("List of documents:", os.listdir("/sd"))

# write to a file
def write_file(filename, data):
    with open("/sd/" + filename, "w") as file:
        file.write(data)
    print("Data has been written to file:", filename)

# Read file
def read_file(filename):
    with open("/sd/" + filename, "r") as file:
        data = file.read()
    print("readout:", data)
    return data

# Example: Initialize SD card and read/write files
def main():
    init_sd()
    
    filename = "example.txt"
    data = "Hello, SD Card!"
    
    write_file(filename, data)
    read_file(filename)

if __name__ == "__main__":
    main()


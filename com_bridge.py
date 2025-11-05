import serial
import threading

# Configure your COM ports and baud rates
COM_A = 'COM8'   # From device A
COM_B = 'COM6'   # To device B
BAUD = 115200

ser_a = serial.Serial(COM_A, BAUD, timeout=0)
ser_b = serial.Serial(COM_B, BAUD, timeout=0)

def forward(src, dst):
    while True:
        data = src.read(1024)
        if data:
            dst.write(data)
            # Optional: print echoed data to monitor
            print(data.decode(errors='ignore'), end='')

threading.Thread(target=forward, args=(ser_a, ser_b), daemon=True).start()
threading.Thread(target=forward, args=(ser_b, ser_a), daemon=True).start()

print(f"Serial bridge running between {COM_A} <-> {COM_B}")
print("Press Ctrl+C to stop.")
while True:
    pass

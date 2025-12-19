import serial
import time
import threading
import sys
import paho.mqtt.client as mqtt

# ---------------- CONFIG ----------------

SERIAL_PORT = "/dev/ttyACM0"
SERIAL_BAUD = 9600

MQTT_BROKER = "broker.mqtt-dashboard.com"
MQTT_SUB_TOPIC = "esiot-2025/blagoja"
MQTT_CMD_TOPIC = "esiot-2025/blagoja/cmd"

# ----------------------------------------

ser = serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=0.1)

# ---------- SERIAL READ THREAD ----------

def serial_reader():
    while True:
        try:
            if ser.in_waiting:
                line = ser.readline().decode().strip()
                print(f"[UNO] {line}")
            time.sleep(0.05)
        except Exception as e:
            print("Serial read error:", e)

# ---------- MQTT CALLBACKS ----------

def on_connect(client, userdata, flags, rc):
    print("[MQTT] Connected")
    client.subscribe(MQTT_SUB_TOPIC)
    client.subscribe(MQTT_CMD_TOPIC)

def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    print(f"[MQTT] {msg.topic}: {payload}")

    # Example: forward distance messages to UNO
    if msg.topic == MQTT_SUB_TOPIC and payload.startswith("distance="):
        dist = payload.split("=")[1]
        ser.write(f"DIST:{dist}\n".encode())

    # Direct command forwarding
    if msg.topic == MQTT_CMD_TOPIC:
        ser.write((payload + "\n").encode())

# ---------- CLI INPUT THREAD ----------

def cli_input():
    print("Type commands (DIST:x, UNCONNECTED, MODE:AUTO, MOTOR:45)")
    while True:
        try:
            cmd = sys.stdin.readline().strip()
            if cmd:
                ser.write((cmd + "\n").encode())
        except Exception as e:
            print("CLI error:", e)

# ---------- MAIN ----------

mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect(MQTT_BROKER, 1883)
mqtt_client.loop_start()

threading.Thread(target=serial_reader, daemon=True).start()
threading.Thread(target=cli_input, daemon=True).start()

print("Backend running...")

while True:
    time.sleep(1)
import serial
import time
import threading
import sys
import paho.mqtt.client as mqtt
from flask import Flask, jsonify, request

# ------------- FLASK APP ----------------
app = Flask(__name__)
system_state = {
    "state": "AUTOMATIC",
    "water_level": None,
    "motor": 0
}
ser_lock = threading.Lock()

def ser_send(s:str):
    with ser_lock:
        ser.write((s + "\n").encode())
        ser.flush()
@app.route("/status", methods=["GET"])
def get_status():
    return jsonify(system_state)
@app.route("/mode", methods=["POST"])
def set_mode():
    mode = request.json.get("mode")
    if mode in ["AUTOMATIC", "MANUAL"]:
        ser_send(f"MODE:{mode}")
        system_state["state"] = mode
        return jsonify({"ok": True, "requested:": mode})
    return jsonify({"error": "invalid mode"}), 400
@app.route("/motor", methods=["POST"])
def set_motor():
    pos = int(request.json.get("position", -1))
    if 0 <= pos <= 90:
        ser_send(f"MOTOR:{pos}")
        system_state["motor"] = pos
        return jsonify({"ok": True})
    return jsonify({"error": "invalid position"}), 400
# ---------------- CONFIG ----------------

SERIAL_PORT = "/dev/ttyACM0"
SERIAL_BAUD = 115200

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
                line = ser.readline().decode().strip().upper()
                print(f"[UNO] {line}")
                if line.startswith("STATE:"):
                        state = line.split(":",1)[1].strip().upper()
                        if state in ("AUTOMATIC","MANUAL"):
                            system_state["state"] = state
                        else:
                            print("[WARN] Unknown state from UNO:",repr(state))
                if line.startswith("LEVEL:"):
                        try:
                            system_state["water_level"] = float(line.split(":")[1])    
                        except ValueError:    
                            print("[WARN] Bad LEVEL:",repr(line))
            time.sleep(0.05)
        except Exception as e:
            print("Serial read error:", e)
# check 
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
        ser.flush()

    # Direct command forwarding
    if msg.topic == MQTT_CMD_TOPIC:
        ser.write((payload + "\n").encode())
        ser.flush()

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

if __name__ == "__main__":
    threading.Thread(target=serial_reader, daemon=True).start()
    threading.Thread(target=cli_input, daemon=True).start()
    mqtt_client.loop_start()
    app.run(host='0.0.0.0',port=5000,debug=False,threaded=True)

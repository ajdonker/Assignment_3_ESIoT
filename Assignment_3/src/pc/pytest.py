import serial
import time
import threading
import sys
import paho.mqtt.client as mqtt
from flask import Flask, jsonify, request

T2 = 10
# ------------- FLASK APP ----------------
app = Flask(__name__)
system_state = {
    "state": "AUTOMATIC",
    "state_requested": None,
    "water_level": None,
    "motor": 0,
    "control": "POT",
    "control_requested": None,
    "gui_last_seen": time.time(),
    "tms_last_seen": time.time()
}
ser_lock = threading.Lock()

def ser_send(s:str):
    with ser_lock:
        ser.write((s + "\n").encode())
        ser.flush()
@app.route("/status", methods=["GET"])
def get_status():
    system_state["gui_last_seen"] = time.time()
    return jsonify(system_state)
@app.route("/mode", methods=["POST"])
def set_mode():
    mode = request.json.get("mode")
    if mode in ["AUTOMATIC", "MANUAL"]:
        ser_send(f"MODE:{mode}")
        #system_state["state"] = mode wait for uno R-R to confirm state 
        system_state["state_requested"] = mode
        return jsonify(ok = True, requested = mode)
    return jsonify({"error": "invalid mode"}), 400
@app.route("/motor", methods=["POST"])
def set_motor():
    if system_state.get("state") == "MANUAL" and system_state.get("control") == "REMOTE":
        pos = int(request.json.get("position", -1))
        # check that the GUI slider converts 0-100 into 0-90 
        if 0 <= pos <= 90:
            ser_send(f"MOTOR:{pos}")
            system_state["motor"] = pos
            return jsonify({"ok": True})
        return jsonify({"error": "invalid position"}), 400
    else:
        return jsonify(error="remote control not owned"),400
@app.route("/control",methods=["POST"])
def set_control():
    src = request.json.get("control")
    if src in ["POT","REMOTE"]:
        ser_send(f"CONTROL:{src}")
        system_state["control_requested"] = src
        return jsonify(ok=True,requested=src)
    return jsonify(error="invalid source"),400
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
                        if state in ("AUTOMATIC","MANUAL","UNCONNECTED"):
                            system_state["state"] = state
                            system_state["state_requested"] = None
                        else:
                            print("[WARN] Unknown state from UNO:",repr(state))
                if line.startswith("LEVEL:"):
                        try:
                            system_state["water_level"] = float(line.split(":")[1])    
                        except ValueError:    
                            print("[WARN] Unknown level from UNO:",repr(line))
                if line.startswith("CONTROL:"):
                    ctrl = line.split(":",1)[1].strip().upper()
                    if ctrl in ("POT","REMOTE"):
                        system_state["control"] = ctrl
                        system_state["state_requested"] = None
                    else:
                        print("[WARN] Unknown control from UNO: ",repr(ctrl))
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
    #print(f"[MQTT] {msg.topic}: {payload}")
    system_state["tms_last_seen"] = time.time()
    if system_state.get("state") == "UNCONNECTED":
        system_state["state_requested"] = "AUTOMATIC"
        ser_send("MODE:AUTOMATIC")
    if msg.topic == MQTT_SUB_TOPIC and payload.startswith("distance="):
        dist = payload.split("=")[1]
        ser_send(f"DIST:{dist}")

    if msg.topic == MQTT_CMD_TOPIC:
        ser_send(payload)

# ---------- CLI INPUT THREAD ----------

def cli_input():
    print("Type commands (DIST:x, UNCONNECTED, MODE:AUTO, MOTOR:45)")
    while True:
        try:
            cmd = sys.stdin.readline().strip()
            if cmd:
                ser_send(cmd)
        except Exception as e:
            print("CLI error:", e)

# ---------- WATCHDOGS --------
def gui_watchdog():
    while True:
        time.sleep(1)
        last = system_state.get("gui_last_seen",0)
        if system_state["control"] == "REMOTE":
            if (time.time() - last) >= T2:
                if system_state["control"] != "POT":
                    system_state["control_requested"] = "POT"
                    ser_send("CONTROL:POT")
def tms_watchdog():
    while True:
        time.sleep(1)
        last = system_state.get("tms_last_seen")
        if (time.time() - last) >= T2:
            if system_state["state"] != "UNCONNECTED":
                system_state["state_requested"] = "UNCONNECTED"
                ser_send("MODE:UNCONNECTED")

# ---------- MAIN ----------

mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect(MQTT_BROKER, 1883)

if __name__ == "__main__":
    threading.Thread(target=serial_reader, daemon=True).start()
    threading.Thread(target=cli_input, daemon=True).start()
    threading.Thread(target=gui_watchdog,daemon=True).start()
    threading.Thread(target=tms_watchdog,daemon=True).start()
    mqtt_client.loop_start()
    app.run(host='0.0.0.0',port=5000,debug=False,threaded=True)

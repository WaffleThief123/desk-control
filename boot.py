import network
import time
import mqtt
import machine
from env_loader import load_env

env = load_env()

WIFI_SSID = env.get("WIFI_SSID", "")
WIFI_PASS = env.get("WIFI_PASS", "")

def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        print("Connecting to Wi-Fi...")
        wlan.connect(WIFI_SSID, WIFI_PASS)
        retries = 0
        while not wlan.isconnected() and retries < 20:
            time.sleep(0.5)
            retries += 1
    if wlan.isconnected():
        print("Connected: IP =", wlan.ifconfig()[0])
    else:
        print("Wi-Fi connection failed.")
        machine.reset()

connect_wifi()
mqtt.connect()

# Wait a moment to let USB settle
time.sleep(1)

# Run main manually
try:
    import main
except Exception as e:
    print("Failed to start main.py:", e)

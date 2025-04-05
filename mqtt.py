from umqtt.simple import MQTTClient
import time
import uasyncio as asyncio

client = None
motor_ref = None

TOPIC_CMD = b"foxdesk/cmd"
TOPIC_HEIGHT = b"foxdesk/height"

def connect():
    global client
    try:
        print("🌐 Connecting to MQTT broker with authentication...")
        client = MQTTClient(
            client_id="mqtt-user",
            server="192.168.0.100",
            port=1883,
            user="your_username",
            password="your_password"
        )
        client.set_callback(mqtt_callback)
        client.connect()
        client.subscribe(TOPIC_CMD)
        print("✅ MQTT connected and subscribed to", TOPIC_CMD.decode())
    except Exception as e:
        print("❌ MQTT connection failed:", e)
        client = None

def mqtt_callback(topic, msg):
    global motor_ref
    msg = msg.decode()
    print("📥 MQTT message:", msg)
    if topic == TOPIC_CMD and motor_ref:
        if msg == "up":
            motor_ref.move_up()
        elif msg == "down":
            motor_ref.move_down()
        elif msg == "stop":
            motor_ref.stop()

async def handle_mqtt(motor):
    global motor_ref
    motor_ref = motor

    if client is None:
        print("⚠️ MQTT not connected; skipping message loop.")
        return

    while True:
        try:
            client.check_msg()
        except OSError as e:
            if e.args and e.args[0] == 128:  # ENOTCONN
                print("🔌 MQTT disconnected. Exiting message loop.")
                return
            else:
                print("⚠️ MQTT check_msg error:", e)
        await asyncio.sleep(0.1)

def publish_height(height):
    if client:
        try:
            client.publish(TOPIC_HEIGHT, str(height))
        except OSError as e:
            if e.args and e.args[0] == 128:
                print("🔌 MQTT disconnected. Skipping publish.")
            else:
                print("⚠️ MQTT publish failed:", e)

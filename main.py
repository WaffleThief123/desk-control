import uasyncio as asyncio
from mqtt import handle_mqtt, publish_height
from motor import DeskMotor
from sensor import VL53L1XReader
import sys
import select

motor = DeskMotor()
sensor = VL53L1XReader()

async def read_serial():
    while True:
        if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
            cmd = sys.stdin.readline().strip()
            handle_cmd(cmd)
        await asyncio.sleep(0.1)

def handle_cmd(cmd):
    if cmd == 'u':
        print("Moving up...")
        motor.move_up()
    elif cmd == 'd':
        print("Moving down...")
        motor.move_down()
    elif cmd == 's':
        print("Stopping motor...")
        motor.stop()
    elif cmd == 'h':
        dist = sensor.read_distance()
        print("Height: {} mm".format(dist))
    else:
        print("Unknown command:", cmd)

async def periodic_publish():
    while True:
        h = sensor.read_distance()
        publish_height(h)
        await asyncio.sleep(5)

async def main():
    print("FoxDesk Controller: Starting up...")
    loop = asyncio.get_event_loop()
    loop.create_task(read_serial())
    loop.create_task(handle_mqtt(motor))
    loop.create_task(periodic_publish())
    while True:
        await asyncio.sleep(1)

asyncio.run(main())

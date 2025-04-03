import machine
import time

class DeskMotor:
    def __init__(self):
        self.relay_a = machine.Pin(18, machine.Pin.OUT)
        self.relay_b = machine.Pin(17, machine.Pin.OUT)
        self.stop()

    def stop(self):
        self.relay_a.off()
        self.relay_b.off()

    def move_up(self):
        self.relay_b.off()
        time.sleep(0.01)
        self.relay_a.on()

    def move_down(self):
        self.relay_a.off()
        time.sleep(0.01)
        self.relay_b.on()

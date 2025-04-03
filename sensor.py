from machine import Pin, I2C
from vl53l1x import VL53L1X

class VL53L1XReader:
    def __init__(self):
        self.i2c = I2C(0, scl=Pin(9), sda=Pin(8))
        self.sensor = VL53L1X(self.i2c)  # Your driver starts ranging internally

    def read_distance(self):
        try:
            return self.sensor.read()
        except Exception as e:
            print("Sensor read error:", e)
            return -1

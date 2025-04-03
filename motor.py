import machine
import time
import math

from sensor import VL53L1XReader


class DeskMotor:
    def __init__(self, sensor_reader_class: VL53L1XReader):
        self.sensor_reader_class = sensor_reader_class
        self.relay_a = machine.Pin(18, machine.Pin.OUT)
        self.relay_b = machine.Pin(17, machine.Pin.OUT)
        self.stop()

    def __max_height(self):
        """
        Minimum height in millimetres.
        :return:
        """
        return 670

    def __min_height(self):
        """
        Maximum height in millimetres.
        :return:
        """
        return 1150

    def __gaussian_steps(self):
        return 30

    def __gaussian_timeframe(self):
        """
        Timeframe of a gaussian curve that should be used to reach a target
        :return:
        """
        return 3000

    def __gaussian_acceptable_delta(self):
        """
        Amount of millimetres that should be considered as an acceptable delta to a desired height.
        :return:
        """
        return 10

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

    def move_gaussian(self, desired_height_in_mm):
        # Configuration
        total_time = self.__gaussian_timeframe() / 1000
        steps = self.__gaussian_steps()
        dt = total_time / steps
        mu = total_time / 2
        sigma = 0.5

        current_height = self.sensor_reader_class.read_distance()
        delta = desired_height_in_mm - current_height

        if abs(delta) < 5:
            return  # already close enough

        direction = 1 if delta > 0 else -1
        move = self.move_up if direction == 1 else self.move_down

        # Normalize Gaussian curve to cumulative sum
        gaussian_cdf = []
        for step in range(steps):
            t = step * dt
            velocity = math.exp(-((t - mu) ** 2) / (2 * sigma ** 2))
            gaussian_cdf.append(velocity)
        total_area = sum(gaussian_cdf)
        normalized_cdf = [sum(gaussian_cdf[:i + 1]) / total_area for i in range(steps)]

        # Perform stepwise movement
        for i in range(steps):
            target_height = current_height + normalized_cdf[i] * delta
            current_position = self.sensor_reader_class.read_distance()
            if direction == 1 and current_position >= target_height:
                self.stop()
            elif direction == -1 and current_position <= target_height:
                self.stop()
            else:
                move()
            time.sleep(dt)
            self.stop()  # Stop after each pulse for control

        self.stop()

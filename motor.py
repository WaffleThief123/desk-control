import machine
import time
import math

from sensor import VL53L1XReader


class PID:
    def __init__(self, kp, ki, kd, integral_limit=5000):
        self.kp = kp
        self.ki = ki
        self.kd = kd
        self.integral = 0
        self.prev_error = 0
        self.integral_limit = integral_limit

    def compute(self, error, dt):
        self.integral += error * dt
        self.integral = max(min(self.integral, self.integral_limit), -self.integral_limit)  # anti-windup
        derivative = (error - self.prev_error) / dt if dt > 0 else 0
        self.prev_error = error
        output = self.kp * error + self.ki * self.integral + self.kd * derivative
        return output


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
        return 685

    def __min_height(self):
        """
        Maximum height in millimetres.
        :return:
        """
        return 1100

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
        total_time = self.__gaussian_timeframe() / 1000  # convert to seconds
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

        # Precompute Gaussian velocity profile
        gaussian_values = []
        for step in range(steps):
            t = step * dt
            v = math.exp(-((t - mu) ** 2) / (2 * sigma ** 2))
            gaussian_values.append(v)

        max_v = max(gaussian_values)
        normalized = [v / max_v for v in gaussian_values]  # normalize to [0,1]

        # Start motor in correct direction
        move()

        for i in range(steps):
            # Use value to control how long motor runs during this step
            pulse_time = dt * normalized[i]
            time.sleep(pulse_time)
            self.stop()
            # sleep the rest of dt to maintain step timing
            time.sleep(dt - pulse_time)
            move()

        self.stop()

    def __estimate_gains(self, error):
        # Self-tuning based on initial error
        base_kp = 0.02
        base_ki = 0.001
        base_kd = 0.01

        scale = min(max(abs(error) / 200, 0.5), 2.0)
        return PID(base_kp * scale, base_ki * scale, base_kd * scale)

    def move_pid(self, desired_height_in_mm, timeout_ms=3000):
        sample_time = 0.1  # seconds
        max_time = timeout_ms / 1000
        t_start = time.ticks_ms()

        current_height = self.sensor_reader_class.read_distance()
        error = desired_height_in_mm - current_height
        pid = self.__estimate_gains(error)

        last_time = time.ticks_ms()
        last_direction = None

        while abs(error) > 3:
            now = time.ticks_ms()
            dt = (time.ticks_diff(now, last_time)) / 1000.0
            last_time = now

            current_height = self.sensor_reader_class.read_distance()
            error = desired_height_in_mm - current_height
            control = pid.compute(error, dt)

            # Determine relay direction
            if control > 0:
                direction = "up"
            elif control < 0:
                direction = "down"
            else:
                direction = None

            # Change direction only if needed
            if direction != last_direction:
                self.stop()
                if direction:
                    self.__set_direction(direction)
                last_direction = direction

            # End condition
            if time.ticks_diff(now, t_start) > timeout_ms:
                break

            time.sleep(sample_time)

        self.stop()
import machine
import time
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
        return 1100  # mm

    def __min_height(self):
        return 685  # mm

    def __acceptable_delta(self):
        return 10  # mm tolerance

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

    def move_to_height(self, target_height_mm):
        acceptable_delta = self.__acceptable_delta()

        current_height = self.sensor_reader_class.read_distance()
        if abs(target_height_mm - current_height) < acceptable_delta:
            print("Already at desired height.")
            return

        direction = 1 if target_height_mm > current_height else -1
        move = self.move_up if direction > 0 else self.move_down

        print("🔧 Starting fixed-speed move", "up" if direction > 0 else "down")
        move()

        timeout_s = 15
        start_time = time.time()

        while True:
            current_height = self.sensor_reader_class.read_distance()
            delta = target_height_mm - current_height

            if abs(delta) <= acceptable_delta:
                print("✅ Reached target height.")
                break

            if time.time() - start_time > timeout_s:
                print("⏱️ Timed out trying to reach height.")
                break

            time.sleep(0.1)

        self.stop()

    def __estimate_gains(self, error):
        # Self-tuning based on initial error
        base_kp = 0.02
        base_ki = 0.001
        base_kd = 0.01

        scale = min(max(abs(error) / 200, 0.5), 2.0)
        return PID(base_kp * scale, base_ki * scale, base_kd * scale)
    def __set_direction(self, direction: str):
        """Set motor direction without delay"""
        if direction == "up":
            self.relay_b.off()
            self.relay_a.on()
        elif direction == "down":
            self.relay_a.off()
        else:
            self.stop()

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
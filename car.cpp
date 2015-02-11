#include <Servo.h>
#include <Arduino.h>

#include <ci_command.h>

enum direction {
	FORWARD,
	BACKWARD,
};

static bool debug = false;

static void my_pinMode(uint8_t pin, uint8_t mode)
{
	if (debug) {
		Serial.print("# pinMode ");
		Serial.print(mode);
		Serial.print(" to pin ");
		Serial.print(pin);
		Serial.print("\n");
	}

	pinMode(pin, mode);
}

struct Beeper {
	uint8_t pin;
	unsigned tone;
	unsigned long duration;

	Beeper(uint8_t pin, unsigned tone, unsigned long duration):
		pin(pin), tone(tone), duration(duration)
	{

	}

	void init (void) const
	{
		my_pinMode(this->pin, OUTPUT);
	}
};

static void my_digitalWrite(uint8_t pin, uint8_t value)
{
	if (debug) {
		Serial.print("# digitalWrite ");
		Serial.print(value);
		Serial.print(" to pin ");
		Serial.print(pin);
		Serial.print("\n");
	}

	digitalWrite(pin, value);
}

static void my_analogWrite(uint8_t pin, int value)
{
	if (debug) {
		Serial.print("# analogWrite ");
		Serial.print(value);
		Serial.print(" to pin ");
		Serial.print(pin);
		Serial.print("\n");
	}

	analogWrite(pin, value);
}

struct Motor {
	uint8_t control[2];
	uint8_t enable;
	enum direction direction;
	int16_t abs_speed;

	Motor(uint8_t control_pin1, uint8_t control_pin2, uint8_t enable_pin):
		control {control_pin1, control_pin2}, enable(enable_pin)
	{

	}

	void set_direction(enum direction direction)
	{
		if (direction == this->direction)
			return;

		this->direction = direction;

		if (direction == FORWARD) {
			my_digitalWrite(this->control[0], HIGH);
			my_digitalWrite(this->control[1], LOW);
		} else {
			my_digitalWrite(this->control[0], LOW);
			my_digitalWrite(this->control[1], HIGH);
		}
	}

	void init(void)
	{
		my_pinMode(this->control[0], OUTPUT);
		my_pinMode(this->control[1], OUTPUT);
		my_pinMode(this->enable, OUTPUT);

		my_digitalWrite(this->enable, LOW);
		this->set_direction(BACKWARD);
		this->set_speed(0);
	}

	void set_speed(int16_t speed)
	{
		int16_t abs_speed;
		enum direction direction;

		if (speed < 0) {
			direction = BACKWARD;
			abs_speed = -speed;
		} else {
			direction = FORWARD;
			abs_speed = speed;
		}
		if (abs_speed > 0xFF)
			abs_speed = 0xFF;

		if (abs_speed == this->abs_speed &&
				direction == this->direction)
			return;

		this->set_direction(direction);
		this->abs_speed = abs_speed;
		my_analogWrite(this->enable, abs_speed);
	}
};

struct RangedServo {
	uint8_t pin;
	int start;
	int end;
	int angle;
	Servo servo;

	RangedServo(uint8_t pin):
		pin(pin)
	{
		this->start = 0;
		this->end = 180;
	}

	RangedServo(uint8_t pin, int start, int end):
		pin(pin)
	{
		int swap;

		/* get sure that start < end */
		if (start > end) {
			swap = start;
			start = end;
			end = swap;
		}

		if (start < 0)
			start = 0;
		if (end > 180)
			end = 180;

		this->start = start;
		this->end = end;
	}

	void write(int value)
	{
		if (debug) {
			Serial.print("# RangedServo.write ");
			Serial.print(value);
			Serial.print(" to servo attached to pin ");
			Serial.print(this->pin);
			Serial.print("\n");
		}

		this->servo.write(value);
	}

	void set_angle(int angle)
	{
		if (angle < this->start || angle > this->end)
			return;
		if (angle == this->angle)
			return;

		this->angle = angle;
		this->write(this->angle);
	}

	void init(void)
	{
		this->servo.attach(this->pin);
		this->set_angle((this->start + this->end) / 2);
	}
};

static struct Motor left_motor = Motor(2, 4, 3);
static struct Motor right_motor = Motor(7, 8, 5);
static struct RangedServo wheel_direction = RangedServo(6, 51, 140);
//static struct RangedServo wheel_direction = RangedServo(6, 0, 180);
static struct RangedServo camera_x_angle = RangedServo(9);
static struct RangedServo camera_z_angle = RangedServo(10);
static bool lights_off = false;
static const struct Beeper beeper = Beeper(12, 440, 200);
static const int head_lights = 11;
static const int photo_sensor = A0;

static void printer(const char *msg)
{
	Serial.print("# ");
	Serial.print(msg);
	Serial.print("\n");
}

void setup(void)
{

	Serial.begin(115200);
	Serial.setTimeout(10);

	left_motor.init();
	right_motor.init();
	wheel_direction.init();
	camera_x_angle.init();
	camera_z_angle.init();

	beeper.init();

	my_pinMode(head_lights, OUTPUT);
	my_digitalWrite(head_lights, HIGH);

	my_pinMode(13, OUTPUT);
	my_digitalWrite(13, LOW);

	printer("init done");
}

static void check_lights(void)
{
	int light;

	light = analogRead(photo_sensor);
	if (debug) {
		Serial.print("photo sensor value :");
		Serial.print(light);
		Serial.print(" lights ");
		Serial.print(lights_off ? "on" : "off");
		Serial.print("\r\n");
	}
	if (lights_off) {
		if (light < 450) {
			lights_off = false;
			my_digitalWrite(head_lights, !lights_off);
		}
	} else {
		if (light > 550) {
			lights_off = true;
			my_digitalWrite(head_lights, !lights_off);
		}
	}
}

void loop(void)
{
	struct ci_command cmd;
	int ret;

	check_lights();

	memset(&cmd, 0, sizeof(cmd));

	ret = Serial.readBytes((char *)&cmd, CI_COMMAND_LENGTH);
	if (ret == 0)
		return;

	if (!ci_command_is_valid(&cmd)) {
		Serial.print("dropped invalid message\n");
		return;
	}

	ci_command_dump(&cmd, printer);

	/* apply the settings */
	left_motor.set_speed(cmd.left_motor_speed);
	right_motor.set_speed(cmd.right_motor_speed);
	wheel_direction.set_angle(cmd.servo_angle);
	camera_x_angle.set_angle(cmd.camera_x_angle);
	camera_z_angle.set_angle(cmd.camera_z_angle);
	if (cmd.beep)
		tone(beeper.pin, beeper.tone, beeper.duration);
}

#ifndef EXTERNAL_MAIN

int main(void)
{
	setup();

	while (true)
		loop();

	return EXIT_SUCCESS;
}

#endif /* EXTERNAL_MAIN */

#include <ESP32Servo.h>
#include <Bluepad32.h>

ControllerPtr myController = nullptr; 

#define BUTTON_X 0x001
#define BUTTON_O 0x002
#define BUTTON_Tri 0x008

#define button_roller BUTTON_X
#define button_servo_lock BUTTON_Tri
#define button_servo_open BUTTON_O

const int SERVO1_PIN = 4;
const int SERVO2_PIN = 16;

const int SERVO1_CHANNEL = 2;
const int SERVO2_CHANNEL = 3;

int vA = 0, vB = 0;
float targetRPM_A = 0, targetRPM_B = 0;

int ENA = 33; 
int IN1 = 25;
int IN2 = 26;
int IN3 = 27;
int IN4 = 14;
int ENB = 13;

const int MOTORA_CHANNEL = 0;
const int MOTORB_CHANNEL = 1;

int PWM_Roller = 19; 
int IN_Roller_1 = 17;
int IN_Roller_2 = 18;

int EncoderB_1 = 22;
int EncoderB_2 = 23;
int EncoderA_1 = 35;
int EncoderA_2 = 34;

unsigned long lastTime;

volatile long encoderCount1;
volatile long lastEncoderCount1;

volatile long encoderCount2;
volatile long lastEncoderCount2;

const float pulsesPerRevolution = 1052.0;

float RPM_A;
float RPM_B;

const int MIN_PWM = 0;
const int MAX_PWM = 255;
const int MAX_RPM = 100;

void onConnectedController(ControllerPtr ctl) {
    if (myController == nullptr) {
        ControllerProperties properties = ctl->getProperties();
        Serial.printf("Controller model: %s, VID=0x%04x, PID=0x%04x\n", ctl->getModelName().c_str(), properties.vendor_id,
                       properties.product_id);
        myController = ctl;
    }
    else {
        Serial.println("CALLBACK: Controller connected");
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    if (myController == ctl) {
        Serial.println("CALLBACK: Controller disconnected");
        myController = nullptr;
    }
}

void setServoAngle(int channel, int angle) {
    int pulseWidth = map(angle, 0, 180, 500, 2400); 
    int duty = map(pulseWidth, 0, 20000, 0, 255);   
    ledcWrite(channel, duty);
}

void IRAM_ATTR readEncoder1() {
    bool currentStateA = digitalRead(EncoderA_1);
    bool currentStateB = digitalRead(EncoderA_2);

    if (currentStateA == currentStateB) {
        encoderCount1++; // Clockwise
        
    } else {
        encoderCount1--; // Counter-clockwise
    }
}

void IRAM_ATTR readEncoder2() {
    bool currentStateA = digitalRead(EncoderB_1);
    bool currentStateB = digitalRead(EncoderB_2);

    if (currentStateA == currentStateB) {
        encoderCount2++; // Clockwise
        
    } else {
        encoderCount2--; // Counter-clockwise
    }
}

void updateRPM() {
    unsigned long currentTime = millis();
    if (currentTime - lastTime >= 100) {
        noInterrupts();
        long currentCount1 = encoderCount1;
        long currentCount2 = encoderCount2;
        interrupts();

        long countDifference1 = currentCount1 - lastEncoderCount1;
        long countDifference2 = currentCount2 - lastEncoderCount2;

        float timeInterval = (currentTime - lastTime) / 1000.0;
        RPM_A = ((countDifference1 / (float)pulsesPerRevolution) * (60.0 / timeInterval));
        RPM_B = ((countDifference2 / (float)pulsesPerRevolution) * (60.0 / timeInterval));

        lastTime = currentTime;
        lastEncoderCount1 = currentCount1;
        lastEncoderCount2 = currentCount2;

        Serial.println("RPM A : " + String(RPM_A) + "| RPM B : " + String(RPM_B));
    }
}

// Convert target RPM to PWM value
int rpmToPWM(float targetRPM) {
    if (targetRPM == 0) return 0;
    
    int pwm = map(abs(targetRPM), 0, MAX_RPM, MIN_PWM, MAX_PWM);
    return constrain(pwm, 0, MAX_PWM);
}

void controlMotors() {
    // motor a control
    if (targetRPM_A > 0) {
        // Forward
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        ledcWrite(MOTORA_CHANNEL, rpmToPWM(targetRPM_A));
    } else if (targetRPM_A < 0) {
        // Backward
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        ledcWrite(MOTORA_CHANNEL, rpmToPWM(targetRPM_A));
    } else {
        // Stop
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
        ledcWrite(MOTORA_CHANNEL, 0);
    }
    
    // motor b control
    if (targetRPM_B > 0) {
        // Forward
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, HIGH);
        ledcWrite(MOTORB_CHANNEL, rpmToPWM(targetRPM_B));
    } else if (targetRPM_B < 0) {
        // Backward
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, LOW);
        ledcWrite(MOTORB_CHANNEL, rpmToPWM(targetRPM_B));
    } else {
        // Stop
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, LOW);
        ledcWrite(MOTORB_CHANNEL, 0);
    }
}

// Process gamepad input and set target RPM
void processGamepad(ControllerPtr ctl) {

    // Handle roller control
    if (ctl->buttons() & button_roller) {
        // digitalWrite(IN_Roller_1, HIGH);
        // digitalWrite(IN_Roller_2, LOW);
        digitalWrite(PWM_Roller, 255);
        Serial.println("Roller diaktifkan");
    } else {
        // digitalWrite(IN_Roller_1, HIGH);
        // digitalWrite(IN_Roller_2, LOW);
        digitalWrite(PWM_Roller, 0);
    }

    // Handle servo lock with LEDC
    if (ctl->buttons() & button_servo_lock) {
        setServoAngle(SERVO1_CHANNEL, 100);
        Serial.println("Sistem Pembuangan diaktifkan");
    } else {
        setServoAngle(SERVO1_CHANNEL, 0);
    }

    // Handle servo open with LEDC
    if (ctl->buttons() & button_servo_open) {
        setServoAngle(SERVO2_CHANNEL, 90);
        Serial.println("Sistem Penutup diaktifkan");
    } else {
        setServoAngle(SERVO2_CHANNEL, 0);
    }

    // Handle motor - convert joystick input to target RPM (0-100)
    int throttle = map(ctl->throttle(), 0, 1000, 0, 100);  // gas
    int brake    = map(ctl->brake(),    0, 1000, 0, 100);  // rem

    int speed = throttle - brake;
    speed = constrain(speed, -100, 100);  // RPM range

    int rawTurn = ctl->axisX();
    if (rawTurn > -50 && rawTurn < 50) rawTurn = 0; // Ignore deathzone

    int turn = map(rawTurn, -500, 500, -100, 100);

    // hitung motor kiri & kanan
    if (speed >= 0) {
        // Forward
        targetRPM_A = constrain(speed + turn, -100, 100);
        targetRPM_B = constrain(speed - turn, -100, 100);
    } else {
        // Backward
        targetRPM_A = constrain(speed - turn, -100, 100);
        targetRPM_B = constrain(speed + turn, -100, 100);
    }


    // Jika controller tidak connect
    if (!ctl || !ctl->isConnected()) {
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, LOW);
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, LOW);
        ledcWrite(MOTORA_CHANNEL, 0);
        ledcWrite(MOTORB_CHANNEL, 0);
        return;
    }
}

void processControllers() {
    if (myController && myController->isConnected() && myController->hasData()) {
        if (myController->isGamepad()) {
            processGamepad(myController);
        } else {
            Serial.println("Unsupported controller");

            targetRPM_A = 0;
            targetRPM_B = 0;
            digitalWrite(IN1, LOW);
            digitalWrite(IN2, LOW);
            digitalWrite(IN3, LOW);
            digitalWrite(IN4, LOW);
            ledcWrite(MOTORA_CHANNEL, 0);
            ledcWrite(MOTORB_CHANNEL, 0);
            }
    }
}

void setup() {
  Serial.begin(115200);

  // Setup for motor
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  // setup PWM (8-bit = 0–255)
  ledcSetup(MOTORA_CHANNEL, 5000, 8); // channel 0, freq 5kHz, 8-bit
  ledcSetup(MOTORB_CHANNEL, 5000, 8);

  ledcAttachPin(ENA, MOTORA_CHANNEL);
  ledcAttachPin(ENB, MOTORB_CHANNEL);

  // Setup encoder
  pinMode(EncoderA_1, INPUT_PULLUP);
  pinMode(EncoderA_2, INPUT_PULLUP);
  pinMode(EncoderB_1, INPUT_PULLUP);
  pinMode(EncoderB_2, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(EncoderA_1), readEncoder1, RISING);
  attachInterrupt(digitalPinToInterrupt(EncoderB_1), readEncoder2, RISING);

  // Setup LEDC for servos
  ledcSetup(SERVO1_CHANNEL, 50, 8);  // 50 Hz for servo
  ledcSetup(SERVO2_CHANNEL, 50, 8);
  ledcAttachPin(SERVO1_PIN, SERVO1_CHANNEL);
  ledcAttachPin(SERVO2_PIN, SERVO2_CHANNEL);

  setServoAngle(SERVO1_CHANNEL, 0);
  setServoAngle(SERVO2_CHANNEL, 0);

  // Setup for roller
  pinMode(PWM_Roller, OUTPUT);
//   pinMode(IN_Roller_1, OUTPUT); 
//   pinMode(IN_Roller_2, OUTPUT);

  // Setup for bluetooth
  Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
  const uint8_t* addr = BP32.localBdAddress();
  Serial.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

  // Setup the Bluepad32 callbacks
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys();
  BP32.enableVirtualDevice(false);
}

void loop() {
    bool dataUpdated = BP32.update();
    if (dataUpdated) {
        processControllers();
        controlMotors();
    }
    updateRPM();
}
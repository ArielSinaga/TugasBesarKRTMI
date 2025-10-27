#include <ESP32Servo.h>
#include <Bluepad32.h>

int vkiri, vkanan;
 
int ENA = 34; 
int IN1 = 25;
int IN2 = 26;
int IN3 = 27;
int IN4 = 14;
int ENB = 35; 

Servo PWMServo1;
Servo PWMServo2;

int Encoder1A = 35;
int Encoder1B = 34;
int Encoder2A = 22;
int Encoder2B = 23;

int PWMA = 19; 
int AI1 = 17;
int AI2 = 18;

int SetpointRPMA = 100, SetpointRPMB = 100;

long prevTA = 0;
int posPrevA = 0;
long prevTB = 0;
int posPrevB = 0;
volatile long encoderCountA = 0;
volatile long encoderCountB = 0;

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

void readEncoderA() {
    int b = digitalRead(ENA);
    int increment = 0;
    if (b > 0) {
        increment = 1;
    }
    else {
        increment = -1;
    }
    encoderCountA += increment;
}

void readEncoderB() {
    int d = digitalRead(ENB);
    int increment = 0;
    if (d > 0) {
        increment = 1;
    }
    else {
        increment = -1;
    }
    encoderCountB += increment;
}

// This callback gets called any time a new gamepad is connected.
// Up to 4 gamepads can be connected at the same time.
void onConnectedController(ControllerPtr ctl) {
    bool foundEmptySlot = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            Serial.printf("CALLBACK: Controller is connected, index=%d\n", i);
            // Additionally, you can get certain gamepad properties like:
            // Model, VID, PID, BTAddr, flags, etc.
            ControllerProperties properties = ctl->getProperties();
            Serial.printf("Controller model: %s, VID=0x%04x, PID=0x%04x\n", ctl->getModelName().c_str(), properties.vendor_id,
                           properties.product_id);
            myControllers[i] = ctl;
            foundEmptySlot = true;
            break;
        }
    }
    if (!foundEmptySlot) {
        Serial.println("CALLBACK: Controller connected, but could not found empty slot");
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    bool foundController = false;

    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            Serial.printf("CALLBACK: Controller disconnected from index=%d\n", i);
            myControllers[i] = nullptr;
            foundController = true;
            break;
        }
    }

    if (!foundController) {
        Serial.println("CALLBACK: Controller disconnected, but not found in myControllers");
    }
}

void dumpGamepad(ControllerPtr ctl) {
    Serial.printf(
        "idx=%d, dpad: 0x%02x, buttons: 0x%04x, axis L: %4d, %4d, axis R: %4d, %4d, brake: %4d, throttle: %4d, "
        "misc: 0x%02x, gyro x:%6d y:%6d z:%6d, accel x:%6d y:%6d z:%6d\n",
        ctl->index(),        // Controller Index
        ctl->dpad(),         // D-pad
        ctl->buttons(),      // bitmask of pressed buttons
        ctl->axisX(),        // (-511 - 512) left X Axis
        ctl->axisY(),        // (-511 - 512) left Y axis
        ctl->axisRX(),       // (-511 - 512) right X axis
        ctl->axisRY(),       // (-511 - 512) right Y axis
        ctl->brake(),        // (0 - 1023): brake button
        ctl->throttle(),     // (0 - 1023): throttle (AKA gas) button
        ctl->miscButtons(),  // bitmask of pressed "misc" buttons
        ctl->gyroX(),        // Gyro X
        ctl->gyroY(),        // Gyro Y
        ctl->gyroZ(),        // Gyro Z
        ctl->accelX(),       // Accelerometer X
        ctl->accelY(),       // Accelerometer Y
        ctl->accelZ()        // Accelerometer Zh
    );
}

void processGamepad(ControllerPtr ctl) {
    if (ctl->a()) {
        static int colorIdx = 0;
        switch (colorIdx % 3) {
            case 0:
                // Red
                ctl->setColorLED(255, 0, 0);
                break;
            case 1:
                // Green
                ctl->setColorLED(0, 255, 0);
                break;
            case 2:
                // Blue
                ctl->setColorLED(0, 0, 255);
                break;
        }
        colorIdx++;
    }

    if (ctl->b()) {
        // Turn on the 4 LED. Each bit represents one LED.
        static int led = 0;
        led++;
        ctl->setPlayerLEDs(led & 0x0f);
    }

    if (ctl->x()) {
        // Some gamepads like DS3, DS4, DualSense, Switch, Xbox One S, Stadia support rumble.
        ctl->playDualRumble(0 /* delayedStartMs */, 250 /* durationMs */, 0x80 /* weakMagnitude */,
                            0x40 /* strongMagnitude */);
    }
    if (ctl->buttons() == 0x0001) {
        digitalWrite(AI1, HIGH);
        digitalWrite(AI2, LOW);
        analogWrite(PWMA, 150);
        Serial.println("Roller diaktifkan");
    }

    if (ctl->buttons() != 0x0001) {
        digitalWrite(AI1, LOW);
        digitalWrite(AI2, LOW);
        analogWrite(PWMA, 0);
        Serial.println("Roller dinonaktifkan");
    }

    if (ctl->buttons() == 0x0002) {
        PWMServo1.setPeriodHertz(50);
        PWMServo1.attach(4);
        PWMServo1.write(180);
        Serial.println("Sistem Pembuangan diaktifkan");
    }

    if (ctl->buttons() != 0x0002) {
        PWMServo1.setPeriodHertz(50);
        PWMServo1.attach(4);
        PWMServo1.write(0);
        Serial.println("Sistem Pembuangan dinonaktifkan");
    }

    if (ctl->buttons() == 0x0003) {
        PWMServo2.setPeriodHertz(50);
        PWMServo2.attach(15);
        PWMServo2.write(0);
        Serial.println("Sistem Penutup diaktifkan");
    }

    if (ctl->buttons() != 0x0003) {
        PWMServo2.setPeriodHertz(50);
        PWMServo2.attach(15);
        PWMServo2.write(180);
        Serial.println("Sistem Penutup dinonaktifkan");
    }

    vkiri = map(ctl->axisX() + ctl->throttle() + ctl->brake(), -25, -508, 70, 255);
    vkanan = map(ctl->axisX() - ctl->throttle() + ctl->brake(), 25, 512, 70, 255);
    
    if (ctl->throttle() > 0 && ctl->brake() == 0) {
        if (ctl->axisX() <= -25){
            digitalWrite(IN1, HIGH);
            digitalWrite(IN2, LOW);
            digitalWrite(IN3, HIGH);
            digitalWrite(IN4, LOW);
            analogWrite(ENA, 0);
            analogWrite(ENB, vkanan);
            Serial.println("Maju ke kiri");
        }
        else if (ctl->axisX() >= 25){
            digitalWrite(IN1, HIGH);
            digitalWrite(IN2, LOW);
            digitalWrite(IN3, HIGH);
            digitalWrite(IN4, LOW);
            analogWrite(ENA, vkiri);
            analogWrite(ENB, 0);
            Serial.println("Maju ke kanan");
        }
        else {
            digitalWrite(IN1, HIGH);
            digitalWrite(IN2, LOW);
            digitalWrite(IN3, HIGH);
            digitalWrite(IN4, LOW);
            analogWrite(ENA, vkiri);
            analogWrite(ENB, vkanan);
            Serial.println("Maju lurus");
        }
    }

    if (ctl->throttle() == 0 && ctl->brake() == 0) {
        if (ctl->axisX() <= -25){
            digitalWrite(IN1, LOW);
            digitalWrite(IN2, LOW);
            digitalWrite(IN3, LOW);
            digitalWrite(IN4, LOW);
            analogWrite(ENA, 0);
            analogWrite(ENB, vkanan);
            Serial.println("Berhenti ke kiri");
        }
        else if (ctl->axisX() >= 25){
            digitalWrite(IN1, LOW);
            digitalWrite(IN2, LOW);
            digitalWrite(IN3, LOW);
            digitalWrite(IN4, LOW);
            analogWrite(ENA, vkiri);
            analogWrite(ENB, 0);
            Serial.println("Berhenti ke kanan");
        }
        else {
            digitalWrite(IN1, LOW);
            digitalWrite(IN2, LOW);
            digitalWrite(IN3, LOW);
            digitalWrite(IN4, LOW);
            analogWrite(ENA, vkiri);
            analogWrite(ENB, vkanan);
            Serial.println("Berhenti lurus");
        }
    }

    if (ctl->brake() > 0 && ctl->throttle() == 0) {
        if (ctl->axisX() <= -25){
            digitalWrite(IN1, LOW);
            digitalWrite(IN2, HIGH);
            digitalWrite(IN3, LOW);
            digitalWrite(IN4, HIGH);
            analogWrite(ENA, 0);
            analogWrite(ENB, vkanan);
            Serial.println("Mundur ke kiri");
        }
        else if (ctl->axisX() >= 25){
            digitalWrite(IN1, LOW);
            digitalWrite(IN2, HIGH);
            digitalWrite(IN3, LOW);
            digitalWrite(IN4, HIGH);
            analogWrite(ENA, vkiri);
            analogWrite(ENB, 0);
            Serial.println("Mundur ke kanan");
        }
        else {
            digitalWrite(IN1, LOW);
            digitalWrite(IN2, HIGH);
            digitalWrite(IN3, LOW);
            digitalWrite(IN4, HIGH);
            analogWrite(ENA, vkiri);
            analogWrite(ENB, vkanan);
            Serial.println("Mundur lurus");
        }
    }

    // See how the different "dump*" functions dump the Controller info.
    dumpGamepad(ctl);
}

void processControllers() {
    for (auto myController : myControllers) {
        if (myController && myController->isConnected() && myController->hasData()) {
            if (myController->isGamepad()) {
                processGamepad(myController);
            } else {
                Serial.println("Unsupported controller");
            }
        }
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(ENA, INPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);
    pinMode(ENB, INPUT);

    pinMode(Encoder1A, INPUT);
    pinMode(Encoder1B, INPUT);
    pinMode(Encoder2A, INPUT);
    pinMode(Encoder2B, INPUT);

    pinMode(PWMA, OUTPUT);
    pinMode(AI1, OUTPUT);
    pinMode(AI2, OUTPUT);

    attachInterrupt(digitalPinToInterrupt(ENA), readEncoderA, RISING);
    attachInterrupt(digitalPinToInterrupt(ENB), readEncoderB, RISING);

    PWMServo1.write(4);
    PWMServo2.write(15);

    Serial.println("Setpoint,RPM");

    Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
    const uint8_t* addr = BP32.localBdAddress();
    Serial.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    // Setup the Bluepad32 callbacks
    BP32.setup(&onConnectedController, &onDisconnectedController);

    // "forgetBluetoothKeys()" should be called when the user performs
    // a "device factory reset", or similar.
    BP32.forgetBluetoothKeys();

    // Enables mouse / touchpad support for gamepads that support them.
    // When enabled, controllers like DualSense and DualShock4 generate two connected devices:
    // - First one: the gamepad
    // - Second one, which is a "virtual device", is a mouse.
    BP32.enableVirtualDevice(false);
}

void loop() {
    // This call fetches all the controllers' data.
    // Call this function in your main ;
    bool dataUpdated = BP32.update();
    if (dataUpdated) {
        processControllers();
    }
    delay(150);

    long currTA = micros();
    float deltaTA = (float)(currTA - prevTA) / 1e6;   
    long deltaCountA = encoderCountA - posPrevA;

    float pulsesPerRevolution = 1085;  

    float RPMA = (deltaCountA / deltaTA) * (60.0 / pulsesPerRevolution);

    float RPMTA = SetpointRPMA;

    float kp = 200;
    float ki = 90;
    float kd = 0;
    float ederivA = 0;
    float eintegralA = 0;
    float ederivB = 0;
    float eintegralB = 0;

    float eA = RPMA - RPMTA;
    ederivA += eA/deltaTA;
    eintegralA += eA*deltaTA;
    float RPMUA = kp*eA + ki*eintegralA + kd*ederivA;

    // Serial.print(SetpointRPMA);
    // Serial.print(",");
    // Serial.println(RPMA);
    // Serial.print(",");
    // Serial.println(RPMUA);

    posPrevA = encoderCountA;
    prevTA = currTA;

    long currTB = micros();
    float deltaTB = (float)(currTB - prevTB) / 1e6;   
    long deltaCountB = encoderCountB - posPrevB;
 
    float RPMB = (deltaCountB / deltaTB) * (60.0 / pulsesPerRevolution);

    float RPMTB = SetpointRPMB;

    float eB = RPMB - RPMTB;
    ederivB += eB/deltaTB;
    eintegralB += eB*deltaTB;
    float RPMUB = kp*eB + ki*eintegralB + kd*ederivB;

    // Serial.print(SetpointRPMB);
    // Serial.print(",");
    // Serial.println(RPMB);
    // Serial.print(",");
    // Serial.println(RPMUB);

    posPrevB = encoderCountB;
    prevTB = currTB;
}

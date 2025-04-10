// Servo and motor control pins for ESP32
#include <ESP32Servo.h>

// Servo pins - using GPIO numbers
#define CHAMBER_SERVO_PIN 18  // GPIO18 for rotation servo
#define DISPENSE_SERVO_PIN 19 // GPIO19 for dispense servo

// DC motor pins
#define MOTOR_FORWARD_PIN 25  // GPIO25 for motor forward
#define MOTOR_BACKWARD_PIN 26 // GPIO26 for motor backward
#define MOTOR_ENABLE_PIN 27   // GPIO27 for motor PWM control

// Servo objects
Servo chamberServo;
Servo dispenseServo;

// Function to initialize servos and motor
void initializeActuators()
{
    // Allocate ESP32 timers for servo control
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    chamberServo.setPeriodHertz(50);  // Standard 50Hz servo
    dispenseServo.setPeriodHertz(50); // Standard 50Hz servo

    chamberServo.attach(CHAMBER_SERVO_PIN, 500, 2400); // Adjust min/max pulse width if needed
    dispenseServo.attach(DISPENSE_SERVO_PIN, 500, 2400);

    // Set initial positions for servos
    chamberServo.write(0);   // Start at position 0
    dispenseServo.write(90); // Neutral position for dispensing servo

    // Configure motor pins
    pinMode(MOTOR_FORWARD_PIN, OUTPUT);
    pinMode(MOTOR_BACKWARD_PIN, OUTPUT);
    pinMode(MOTOR_ENABLE_PIN, OUTPUT);

    // Stop the motor initially
    digitalWrite(MOTOR_FORWARD_PIN, LOW);
    digitalWrite(MOTOR_BACKWARD_PIN, LOW);
    analogWrite(MOTOR_ENABLE_PIN, 0);

    Serial.println("Actuators initialized");
}

// Function to rotate the chamber to the desired compartment
void rotateChamberTo(int compartment)
{
    // Assuming 6 compartments (0-5), each 60 degrees apart
    // Ensure compartment is in range
    if (compartment < 0)
        compartment = 0;
    if (compartment > 5)
        compartment = 5;

    int angle = compartment * 60; // Calculate the angle for the desired compartment
    Serial.print("Rotating chamber to compartment: ");
    Serial.print(compartment);
    Serial.print(" (angle: ");
    Serial.print(angle);
    Serial.println(" degrees)");

    chamberServo.write(angle);
    delay(1000); // Wait for the servo to reach the position
}

// Function to dispense a pill
void dispensePill()
{
    Serial.println("Dispensing pill...");
    dispenseServo.write(0);  // Rotate to dispense position
    delay(1000);             // Wait for the pill to drop
    dispenseServo.write(90); // Return to neutral position
    delay(500);              // Wait for the servo to stabilize
}

// Function to move the tray forward
void moveTrayForward()
{
    Serial.println("Moving tray forward...");
    digitalWrite(MOTOR_FORWARD_PIN, HIGH);
    digitalWrite(MOTOR_BACKWARD_PIN, LOW);
    analogWrite(MOTOR_ENABLE_PIN, 255); // Full speed
    delay(2000);                        // Adjust delay based on the distance to move
    analogWrite(MOTOR_ENABLE_PIN, 0);   // Stop the motor
}

// Function to move the tray backward
void moveTrayBackward()
{
    Serial.println("Moving tray backward...");
    digitalWrite(MOTOR_FORWARD_PIN, LOW);
    digitalWrite(MOTOR_BACKWARD_PIN, HIGH);
    analogWrite(MOTOR_ENABLE_PIN, 255); // Full speed
    delay(2000);                        // Adjust delay based on the distance to move
    analogWrite(MOTOR_ENABLE_PIN, 0);   // Stop the motor
}

// Function to move the tray back and forth as a test sequence
void testActuators()
{
    Serial.println("Starting actuator test sequence...");

    // Test chamber rotation
    for (int i = 0; i <= 5; i++)
    {
        rotateChamberTo(i);
        delay(500);
    }

    // Return to chamber 0
    rotateChamberTo(0);
    delay(1000);

    // Test pill dispensing
    dispensePill();
    delay(1000);

    // Test tray movement
    moveTrayForward();
    delay(1000);
    moveTrayBackward();

    Serial.println("Test sequence completed");
    // ** Example usage:
    // rotateChamberTo(medications[i].chamber); // Rotate to the correct chamber
    // delay(1000);
    // dispensePill();    // Dispense the pill
    // moveTrayForward(); // Move tray forward to collect pill
    // delay(2000);
    // moveTrayBackward(); // Return tray to starting position
}
#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <ArduinoJson.h>
#include "time.h"
#include "esp_sntp.h"

// WiFi credentials
#define WIFI_SSID "Hydra_Wlan$0"
#define WIFI_PASSWORD "Scienhac01"

// Firebase project credentials
#define API_KEY "AIzaSyAnanb7oZ0LmlQrjb31NJLIxLi_GKPjBB4"
#define DATABASE_URL "https://iot-prj-ac910-default-rtdb.firebaseio.com/"

// Servo pins - using GPIO numbers
#define CHAMBER_SERVO_PIN 18  // GPIO18 for rotation servo
#define DISPENSE_SERVO_PIN 19 // GPIO19 for dispense servo

// DC motor pins
#define MOTOR_FORWARD_PIN 25  // GPIO25 for motor forward
#define MOTOR_BACKWARD_PIN 26 // GPIO26 for motor backward
#define MOTOR_ENABLE_PIN 27   // GPIO27 for motor PWM control

// IR sensor pin
#define IR_SENSOR_PIN 23 // GPIO23 for IR sensor input

// Buzzer pin
#define BUZZER_PIN 13 // GPIO13 for buzzer

// Buzzer frequencies for different tones
#define NOTE_C5 523
#define NOTE_D5 587
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_G5 784
#define NOTE_A5 880
#define NOTE_B5 988
#define NOTE_C6 1047

// NTP server settings
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
// GMT offset for India is +5:30 = 19800 seconds
const long gmtOffset_sec = 19800;
// India does not use daylight saving time
const int daylightOffset_sec = 0;
const char *time_zone = "IST-5:30";

// Servo objects
Servo chamberServo;
Servo dispenseServo;

// Firebase Data objects
FirebaseData fbdo1;
FirebaseData fbdo2;
FirebaseAuth auth;
FirebaseConfig config;

// Track if signup was successful
bool signupOK = false;

// IR sensor state
bool pillDetected = false;
bool previousPillState = false;

// Structure to store medication data
struct Medication
{
    int chamber;
    bool dispensed;
    int hour;
    int minute;
    String name;
    String lastDispensed;
};

// Array to store medications
Medication medications[10]; // Assuming max 10 medications
int medicationCount = 0;

// Structure to store pill dispenser status
struct PillDispenserStatus
{
    bool isOnline;
    bool lastDispenseSuccessful;
    String lastDispenseTime;
    String lastSeen;
} pillDispenserStatus;

// Function prototypes
void initializeActuators();
void connectToWiFi();
void setupFirebase();
void setupNTP();
void printLocalTime();
void timeavailable(struct timeval *t);
void rotateChamberTo(int compartment);
void dispensePill();
void moveTrayForward();
void moveTrayBackward();
bool checkIRSensor();
void testIRSensor();
void testActuators();
void medicationsStreamCallback(FirebaseStream data);
void pillDispenserStreamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);
void printStoredMedications();
void updatePillDispenserStatus(bool dispenseSuccessful);
void checkScheduledMedications();

// Buzzer function prototypes
void initializeBuzzer();
void beep(int frequency, int duration);
void playSuccessTone();
void playErrorTone();
void playMedicationReadyTone();
void playReminderTone();
void playStartupTone();
void testBuzzer();

// Buzzer Functions
void initializeBuzzer()
{
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off initially
    Serial.println("Buzzer initialized");
}

// Simple beep function
void beep(int frequency, int duration)
{
    tone(BUZZER_PIN, frequency, duration);
    delay(duration);
    noTone(BUZZER_PIN);
}

// Success tone - ascending notes
void playSuccessTone()
{
    Serial.println("Playing success tone");
    beep(NOTE_C5, 100);
    delay(50);
    beep(NOTE_E5, 100);
    delay(50);
    beep(NOTE_G5, 100);
    delay(50);
    beep(NOTE_C6, 300);
}

// Error tone - descending notes
void playErrorTone()
{
    Serial.println("Playing error tone");
    beep(NOTE_C6, 100);
    delay(50);
    beep(NOTE_A5, 100);
    delay(50);
    beep(NOTE_F5, 100);
    delay(50);
    beep(NOTE_C5, 300);
}

// Medication ready tone - two beeps
void playMedicationReadyTone()
{
    Serial.println("Playing medication ready tone");
    beep(NOTE_G5, 300);
    delay(200);
    beep(NOTE_G5, 500);
}

// Reminder tone - alternating pattern
void playReminderTone()
{
    Serial.println("Playing reminder tone");
    for (int i = 0; i < 3; i++)
    {
        beep(NOTE_C5, 200);
        delay(200);
        beep(NOTE_G5, 200);
        delay(200);
    }
}

// Startup melody
void playStartupTone()
{
    Serial.println("Playing startup tone");
    int startupMelody[] = {NOTE_C5, NOTE_E5, NOTE_G5, NOTE_C6};
    int noteDurations[] = {100, 100, 100, 300};

    for (int i = 0; i < 4; i++)
    {
        beep(startupMelody[i], noteDurations[i]);
        delay(50);
    }
}

// Test buzzer function
void testBuzzer()
{
    Serial.println("Testing buzzer...");

    // Play startup tone
    playStartupTone();
    delay(1000);

    // Play success tone
    playSuccessTone();
    delay(1000);

    // Play error tone
    playErrorTone();
    delay(1000);

    // Play medication ready tone
    playMedicationReadyTone();
    delay(1000);

    // Play reminder tone
    playReminderTone();

    Serial.println("Buzzer test completed");
}

// Function to update current pill dispenser status in Firebase
void updatePillDispenserStatus(bool dispenseSuccessful)
{
    if (Firebase.ready() && signupOK)
    {
        // Get current time
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo))
        {
            Serial.println("Failed to obtain time");
            return;
        }

        char timeStringBuffer[9]; // HH:MM:SS + null terminator
        strftime(timeStringBuffer, sizeof(timeStringBuffer), "%H:%M:%S", &timeinfo);

        char dateTimeStringBuffer[20]; // YYYY-MM-DD HH:MM:SS + null terminator
        strftime(dateTimeStringBuffer, sizeof(dateTimeStringBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);

        // Update the status
        FirebaseJson json;
        json.set("isOnline", true);
        json.set("lastDispenseSuccessful", dispenseSuccessful);
        json.set("lastDispenseTime", String(timeStringBuffer));
        json.set("lastSeen", String(dateTimeStringBuffer));

        if (Firebase.RTDB.updateNode(&fbdo1, "pillDispenser", &json))
        {
            Serial.println("Pill dispenser status updated successfully");
        }
        else
        {
            Serial.println("Failed to update pill dispenser status:");
            Serial.println(fbdo1.errorReason());
        }
    }
}

// Function to update medication dispensed status
void updateMedicationDispensedStatus(String medicationId, bool dispensed)
{
    if (Firebase.ready() && signupOK)
    {
        // Get current time
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo))
        {
            Serial.println("Failed to obtain time");
            return;
        }

        char dateTimeStringBuffer[20]; // YYYY-MM-DD HH:MM:SS + null terminator
        strftime(dateTimeStringBuffer, sizeof(dateTimeStringBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);

        // Update the medication
        FirebaseJson json;
        json.set("dispensed", dispensed);
        json.set("lastDispensed", String(dateTimeStringBuffer));

        String path = "medications/" + medicationId;
        if (Firebase.RTDB.updateNode(&fbdo1, path, &json))
        {
            Serial.println("Medication status updated successfully");
        }
        else
        {
            Serial.println("Failed to update medication status:");
            Serial.println(fbdo1.errorReason());
        }
    }
}

// Function to check if any medication is scheduled for the current time
void checkScheduledMedications()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return;
    }

    int currentHour = timeinfo.tm_hour;
    int currentMinute = timeinfo.tm_min;

    Serial.print("Current time: ");
    Serial.print(currentHour);
    Serial.print(":");
    Serial.println(currentMinute);

    for (int i = 0; i < medicationCount; i++)
    {
        // Check if this medication is scheduled for the current time and not yet dispensed
        if (medications[i].hour == currentHour &&
            medications[i].minute == currentMinute &&
            !medications[i].dispensed)
        {

            Serial.print("Time to dispense: ");
            Serial.println(medications[i].name);

            // First play the medication reminder tone
            Serial.println("Playing medication alert tone...");
            playMedicationReadyTone();

            // Wait for 5 seconds before starting the dispensing process
            // This gives the user time to approach the device after hearing the alert
            Serial.println("Waiting 5 seconds before dispensing...");
            delay(5000);

            // Perform the dispensing sequence
            rotateChamberTo(medications[i].chamber);
            delay(1000);
            dispensePill();

            // Check if pill was detected by IR sensor
            unsigned long startTime = millis();
            unsigned long timeout = 5000; // 5-second timeout
            bool pillDetected = false;

            Serial.println("Waiting for pill detection...");

            while (millis() - startTime < timeout)
            {
                if (checkIRSensor())
                {
                    pillDetected = true;
                    Serial.println("Pill detected by IR sensor!");
                    break;
                }
                delay(50);
            }

            if (pillDetected)
            {
                moveTrayForward();
                delay(2000);
                moveTrayBackward();

                // Play success tone for successful dispensing
                playSuccessTone();

                // Update Firebase with successful dispense
                updatePillDispenserStatus(true);
                // We don't have medication IDs stored locally, so we can't update the specific medication
                // This would need to be improved in a production version
            }
            else
            {
                Serial.println("Pill dispensing failed - no pill detected!");
                // Play error tone for failed dispensing
                playErrorTone();
                updatePillDispenserStatus(false);
            }
        }
    }
}

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

    // Configure IR sensor pin as input
    pinMode(IR_SENSOR_PIN, INPUT);

    Serial.println("Actuators and sensors initialized");
}

// Function to connect to WiFi
void connectToWiFi()
{
    Serial.print("Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

// Function to set up Firebase connection
void setupFirebase()
{
    // Configure Firebase
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    config.token_status_callback = tokenStatusCallback;

    // Anonymous sign-up
    if (Firebase.signUp(&config, &auth, "", ""))
    {
        Serial.println("Anonymous sign-up successful!");
        signupOK = true;
    }
    else
    {
        Serial.printf("Anonymous sign-up failed: %s\n", config.signer.signupError.message.c_str());
    }

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    // Wait for Firebase to be ready
    while (!Firebase.ready() && signupOK)
    {
        Serial.print(".");
        delay(1000);
    }
    Serial.println();

    if (signupOK)
    {
        // Begin streaming the /medications path
        if (!Firebase.RTDB.beginStream(&fbdo1, "/medications"))
        {
            Serial.print("Failed to begin medications stream: ");
            Serial.println(fbdo1.errorReason().c_str());
        }
        else
        {
            Serial.println("Medications stream started!");
            Firebase.RTDB.setStreamCallback(&fbdo1, medicationsStreamCallback, streamTimeoutCallback);
        }

        // Begin streaming the /pillDispenser path
        if (!Firebase.RTDB.beginStream(&fbdo2, "/pillDispenser"))
        {
            Serial.print("Failed to begin pillDispenser stream: ");
            Serial.println(fbdo2.errorReason().c_str());
        }
        else
        {
            Serial.println("Pill dispenser stream started!");
            Firebase.RTDB.setStreamCallback(&fbdo2, pillDispenserStreamCallback, streamTimeoutCallback);
        }
    }
}

// Function to set up NTP time synchronization
void setupNTP()
{
    // Set notification callback
    sntp_set_time_sync_notification_cb(timeavailable);

    // Set timezone and NTP servers
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

    Serial.println("NTP time sync configured");
}

// Function to print local time
void printLocalTime()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("No time available (yet)");
        return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

// Callback when time is synchronized
void timeavailable(struct timeval *t)
{
    Serial.println("Got time adjustment from NTP!");
    printLocalTime();
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

// Function to check the IR sensor
bool checkIRSensor()
{
    // Read IR sensor (adjust logic based on your sensor type)
    // Most IR sensors output LOW when an object is detected
    return (digitalRead(IR_SENSOR_PIN) == LOW);
}

// Function to test the IR sensor
void testIRSensor()
{
    Serial.println("IR Sensor Test: Please place and remove an object in front of the sensor");

    unsigned long startTime = millis();
    unsigned long testDuration = 10000; // 10-second test

    while (millis() - startTime < testDuration)
    {
        bool currentState = checkIRSensor();

        // State change detection
        if (currentState != previousPillState)
        {
            if (currentState)
            {
                Serial.println("Object detected!");
            }
            else
            {
                Serial.println("Object removed!");
            }
            previousPillState = currentState;
        }

        delay(100); // Short delay to prevent too frequent readings
    }

    Serial.println("IR sensor test completed.");
}

// Test all actuators and sensors
void testActuators()
{
    Serial.println("Starting actuator test sequence...");

    // Test buzzer first
    testBuzzer();

    // Test IR sensor
    testIRSensor();

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
}

// Stream callback function to handle real-time updates for /medications
void medicationsStreamCallback(FirebaseStream data)
{
    Serial.println("------------------------------------");
    Serial.println("Medications Stream Data Received:");
    Serial.println("Path: " + data.dataPath());
    Serial.println("Type: " + data.dataType());
    Serial.println("Data: " + data.stringData());

    if (data.dataType() == "json")
    {
        Serial.println("JSON Data:");
        Serial.println(data.stringData());

        // Always clear previous data and replace with new data
        medicationCount = 0;

        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, data.stringData());

        if (!error)
        {
            // Check if the data is a full medications list or a single medication
            if (data.dataPath() == "/" || data.dataPath() == "")
            {
                // Process as a complete medications list
                JsonObject jsonMedications = doc.as<JsonObject>();

                for (JsonPair kv : jsonMedications)
                {
                    if (medicationCount < 10) // Prevent array overflow
                    {
                        JsonObject med = kv.value().as<JsonObject>();

                        medications[medicationCount].chamber = med["chamber"].as<int>();
                        medications[medicationCount].dispensed = med["dispensed"].as<bool>();
                        medications[medicationCount].hour = med["hour"].as<int>();
                        medications[medicationCount].minute = med["minute"].as<int>();
                        medications[medicationCount].name = med["name"].as<String>();

                        if (med.containsKey("lastDispensed"))
                        {
                            medications[medicationCount].lastDispensed = med["lastDispensed"].as<String>();
                        }
                        else
                        {
                            medications[medicationCount].lastDispensed = "";
                        }

                        String medId = kv.key().c_str();
                        Serial.println("Stored medication: " + medId);
                        Serial.print("Chamber: ");
                        Serial.println(medications[medicationCount].chamber);
                        Serial.print("Name: ");
                        Serial.println(medications[medicationCount].name);

                        medicationCount++;
                    }
                }
            }
            else
            {
                // This is a single medication - still replace everything
                // Clear any previous data
                medicationCount = 1;

                // Store just this medication
                medications[0].chamber = doc["chamber"].as<int>();
                medications[0].dispensed = doc["dispensed"].as<bool>();
                medications[0].hour = doc["hour"].as<int>();
                medications[0].minute = doc["minute"].as<int>();
                medications[0].name = doc["name"].as<String>();

                if (doc.containsKey("lastDispensed"))
                {
                    medications[0].lastDispensed = doc["lastDispensed"].as<String>();
                }
                else
                {
                    medications[0].lastDispensed = "";
                }

                Serial.println("Stored single medication");
                Serial.print("Chamber: ");
                Serial.println(medications[0].chamber);
                Serial.print("Name: ");
                Serial.println(medications[0].name);
            }

            Serial.print("Total medications stored: ");
            Serial.println(medicationCount);
        }
        else
        {
            Serial.print("JSON deserialization failed: ");
            Serial.println(error.c_str());
        }
    }
    Serial.println("------------------------------------");
}

// Stream callback function for /pillDispenser
void pillDispenserStreamCallback(FirebaseStream data)
{
    Serial.println("------------------------------------");
    Serial.println("Pill Dispenser Stream Data Received:");
    Serial.println("Path: " + data.dataPath());
    Serial.println("Type: " + data.dataType());
    Serial.println("Data: " + data.stringData());

    if (data.dataType() == "json")
    {
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, data.stringData());
        if (!error)
        {
            // Simply replace all pill dispenser data with new data
            pillDispenserStatus.isOnline = doc["isOnline"].as<bool>();
            pillDispenserStatus.lastDispenseSuccessful = doc["lastDispenseSuccessful"].as<bool>();
            pillDispenserStatus.lastDispenseTime = doc["lastDispenseTime"].as<String>();
            pillDispenserStatus.lastSeen = doc["lastSeen"].as<String>();

            Serial.println("Pill Dispenser Status Updated:");
            Serial.print("isOnline: ");
            Serial.println(pillDispenserStatus.isOnline ? "true" : "false");
            Serial.print("lastDispenseSuccessful: ");
            Serial.println(pillDispenserStatus.lastDispenseSuccessful ? "true" : "false");
            Serial.print("lastDispenseTime: ");
            Serial.println(pillDispenserStatus.lastDispenseTime);
            Serial.print("lastSeen: ");
            Serial.println(pillDispenserStatus.lastSeen);
        }
        else
        {
            Serial.print("JSON deserialization failed: ");
            Serial.println(error.c_str());
        }
    }
    Serial.println("------------------------------------");
}

// Stream timeout callback
void streamTimeoutCallback(bool timeout)
{
    if (timeout)
        Serial.println("Stream timeout, resuming...");
}

// Function to print all stored medications
void printStoredMedications()
{
    Serial.println("------------------------------------");
    Serial.println("CURRENT MEDICATIONS IN MEMORY:");

    if (medicationCount == 0)
    {
        Serial.println("No medications stored.");
    }
    else
    {
        for (int i = 0; i < medicationCount; i++)
        {
            Serial.print("Medication #");
            Serial.println(i + 1);
            Serial.print("Chamber: ");
            Serial.println(medications[i].chamber);
            Serial.print("Dispensed: ");
            Serial.println(medications[i].dispensed ? "true" : "false");
            Serial.print("Hour: ");
            Serial.println(medications[i].hour);
            Serial.print("Minute: ");
            Serial.println(medications[i].minute);
            Serial.print("Name: ");
            Serial.println(medications[i].name);
            Serial.print("Last Dispensed: ");
            Serial.println(medications[i].lastDispensed);
            Serial.println();
        }
    }
    Serial.println("------------------------------------");
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Initializing SmartPill-Dose System...");

    // Initialize components
    initializeActuators();
    initializeBuzzer(); // Initialize the buzzer
    connectToWiFi();
    setupNTP();
    setupFirebase();

    // Play startup tone to indicate system is ready
    playStartupTone();

    // Update pillDispenser status to show it's online
    if (signupOK)
    {
        FirebaseJson json;
        json.set("isOnline", true);
        json.set("lastSeen", "System startup");

        if (Firebase.RTDB.updateNode(&fbdo1, "pillDispenser", &json))
        {
            Serial.println("Pill dispenser online status updated successfully");
        }
        else
        {
            Serial.println("Failed to update pill dispenser status:");
            Serial.println(fbdo1.errorReason());
        }
    }

    Serial.println("System initialized and ready!");
}

unsigned long lastCheckTime = 0;
unsigned long checkInterval = 30000; // Check every 30 seconds
unsigned long lastStatusUpdateTime = 0;
unsigned long statusUpdateInterval = 60000; // Update status every minute
unsigned long lastReminderTime = 0;
unsigned long reminderInterval = 300000; // Reminder every 5 minutes if pill not taken

void loop()
{
    unsigned long currentMillis = millis();

    // Check for scheduled medications
    if (currentMillis - lastCheckTime >= checkInterval)
    {
        lastCheckTime = currentMillis;

        if (signupOK && Firebase.ready())
        {
            checkScheduledMedications();
        }
    }

    // Update online status periodically
    if (currentMillis - lastStatusUpdateTime >= statusUpdateInterval)
    {
        lastStatusUpdateTime = currentMillis;

        if (signupOK && Firebase.ready())
        {
            // Get current time
            struct tm timeinfo;
            if (getLocalTime(&timeinfo))
            {
                char dateTimeStringBuffer[20]; // YYYY-MM-DD HH:MM:SS + null terminator
                strftime(dateTimeStringBuffer, sizeof(dateTimeStringBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);

                FirebaseJson json;
                json.set("isOnline", true);
                json.set("lastSeen", String(dateTimeStringBuffer));

                Firebase.RTDB.updateNode(&fbdo1, "pillDispenser", &json);
            }
        }
    }

    // Check IR sensor state for real-time detection
    bool currentPillState = checkIRSensor();

    // Detect state change
    if (currentPillState != previousPillState)
    {
        if (currentPillState)
        {
            Serial.println("Pill detected by IR sensor!");
        }
        else
        {
            Serial.println("Pill removed or not detected!");
        }

        // Update previous state
        previousPillState = currentPillState;
    }

    // Small delay to prevent CPU hogging
    delay(100);
}
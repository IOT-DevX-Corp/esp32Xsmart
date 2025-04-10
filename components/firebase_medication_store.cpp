#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <ArduinoJson.h>

// WiFi credentials
#define WIFI_SSID "Hydra_Wlan$0"
#define WIFI_PASSWORD "Scienhac01"

// Firebase project credentials
#define API_KEY "AIzaSyAnanb7oZ0LmlQrjb31NJLIxLi_GKPjBB4"
#define DATABASE_URL "https://iot-prj-ac910-default-rtdb.firebaseio.com/"

// Define Firebase Data objects
FirebaseData fbdo1;
FirebaseData fbdo2;
FirebaseAuth auth;
FirebaseConfig config;

// Track if signup was successful
bool signupOK = false;

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

// Array to store medications (simplified approach)
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
                        medications[medicationCount].lastDispensed = med["lastDispensed"].as<String>();

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
                medications[0].lastDispensed = doc["lastDispensed"].as<String>();

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

void streamTimeoutCallback(bool timeout)
{
    if (timeout)
        Serial.println("Stream timeout, resuming...");
}

void setup()
{
    Serial.begin(115200);

    // Connect to WiFi
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

void loop()
{
    if (signupOK && Firebase.ready())
    {
        // Check connection and print stored data periodically
        if (millis() % 30000 == 0)
        {
            Serial.println("Firebase connection is active");
            printStoredMedications();
        }
    }

    delay(100);
}
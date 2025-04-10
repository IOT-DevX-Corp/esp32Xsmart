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

        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, data.stringData());

        if (!error)
        {
            // Check if we're getting a single medication or a list
            if (data.dataPath().indexOf("-") == 0)
            {
                // Single medication with path like "/-ONTOAqDDApaIG9M6BNn"
                // The data is directly a medication object, not a map of medications
                int chamber = doc["chamber"].as<int>();
                bool dispensed = doc["dispensed"].as<bool>();
                int hour = doc["hour"].as<int>();
                int minute = doc["minute"].as<int>();
                String name = doc["name"].as<String>();
                String lastDispensed = doc["lastDispensed"].as<String>();

                Serial.println("--- New Medication Added ---");
                Serial.print("Chamber: ");
                Serial.println(chamber);
                Serial.print("Dispensed: ");
                Serial.println(dispensed ? "true" : "false");
                Serial.print("Hour: ");
                Serial.println(hour);
                Serial.print("Minute: ");
                Serial.println(minute);
                Serial.print("Name: ");
                Serial.println(name);
                Serial.print("Last Dispensed: ");
                Serial.println(lastDispensed);
            }
            else
            {
                // We're getting the entire medications map
                JsonObject medications = doc.as<JsonObject>();
                for (JsonPair kv : medications)
                {
                    JsonObject medication = kv.value().as<JsonObject>();
                    int chamber = medication["chamber"].as<int>();
                    bool dispensed = medication["dispensed"].as<bool>();
                    int hour = medication["hour"].as<int>();
                    int minute = medication["minute"].as<int>();
                    String name = medication["name"].as<String>();
                    String lastDispensed = medication["lastDispensed"].as<String>();
                    String key = kv.key().c_str();

                    Serial.println("--- Medication: " + key + " ---");
                    Serial.print("Chamber: ");
                    Serial.println(chamber);
                    Serial.print("Dispensed: ");
                    Serial.println(dispensed ? "true" : "false");
                    Serial.print("Hour: ");
                    Serial.println(hour);
                    Serial.print("Minute: ");
                    Serial.println(minute);
                    Serial.print("Name: ");
                    Serial.println(name);
                    Serial.print("Last Dispensed: ");
                    Serial.println(lastDispensed);
                }
            }
        }
        else
        {
            Serial.print("JSON deserialization failed in medicationsStreamCallback: ");
            Serial.println(error.c_str());
        }
    }
    else if (data.dataType() == "add" || data.dataType() == "patch" || data.dataType() == "put")
    {
        Serial.println("Medication updated: " + data.dataPath() + " with data: " + data.stringData());
    }
    else if (data.dataType() == "remove")
    {
        Serial.println("Medication removed at: " + data.dataPath());
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
            bool isOnline = doc["isOnline"].as<bool>();
            bool lastDispenseSuccessful = doc["lastDispenseSuccessful"].as<bool>();
            String lastDispenseTime = doc["lastDispenseTime"].as<String>();
            String lastSeen = doc["lastSeen"].as<String>();

            Serial.println("--- Pill Dispenser Status Updated ---");
            Serial.print("isOnline: ");
            Serial.println(isOnline ? "true" : "false");
            Serial.print("lastDispenseSuccessful: ");
            Serial.println(lastDispenseSuccessful ? "true" : "false");
            Serial.print("lastDispenseTime: ");
            Serial.println(lastDispenseTime);
            Serial.print("lastSeen: ");
            Serial.println(lastSeen);
        }
        else
        {
            Serial.print("JSON deserialization failed in pillDispenserStreamCallback: ");
            Serial.println(error.c_str());
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
    Serial.println();
    Serial.println("ESP32 Firebase Realtime Database Streaming Example");

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    // Assign the API key and database URL (required)
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    // Assign the callback function for token generation
    config.token_status_callback = tokenStatusCallback;

    // Sign up anonymously
    Serial.println("Signing up anonymously...");
    if (Firebase.signUp(&config, &auth, "", ""))
    {
        Serial.println("Anonymous sign-up successful!");
        signupOK = true;
    }
    else
    {
        Serial.printf("Anonymous sign-up failed: %s\n", config.signer.signupError.message.c_str());
    }

    // Initialize Firebase
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    // Wait for Firebase to be ready
    Serial.println("Waiting for Firebase to be ready...");
    unsigned long timeout = millis() + 10000; // 10-second timeout
    while (!Firebase.ready() && millis() < timeout)
    {
        Serial.print(".");
        delay(1000);
    }
    Serial.println();

    if (!Firebase.ready())
    {
        Serial.println("Firebase not ready after timeout!");
    }
    else
    {
        Serial.println("Firebase is ready!");
    }

    // Continue only if signup was successful
    if (signupOK)
    {
        // Begin streaming the /medications path
        Serial.println("Setting up medications stream...");
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
        Serial.println("Setting up pill dispenser stream...");
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

        // Test read to verify database connection
        Serial.println("Testing database connection...");
        if (Firebase.RTDB.getString(&fbdo1, "/"))
        {
            Serial.println("Database connection successful. Data: " + fbdo1.stringData());
        }
        else
        {
            Serial.print("Database connection failed: ");
            Serial.println(fbdo1.errorReason().c_str());
        }
    }
}

void loop()
{
    // Firebase.ready() should be called frequently in the loop
    if (signupOK)
    {
        // No need to poll for data - the callbacks handle that

        // Optional: Check connection status periodically
        if (millis() % 30000 == 0)
        {
            if (Firebase.ready())
            {
                Serial.println("Firebase connection still active");
            }
            else
            {
                Serial.println("Firebase connection lost, attempting to reconnect...");
            }
        }
    }

    delay(100);
}
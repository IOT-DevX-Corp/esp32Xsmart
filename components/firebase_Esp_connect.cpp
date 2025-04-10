#include <WiFi.h>
#include <HTTPClient.h>

// Replace with your network credentials
#define WIFI_SSID "Hydra_Wlan$0"
#define WIFI_PASSWORD "Scienhac01"

// Replace with your Firebase project details
#define FIREBASE_HOST "https://iot-prj-ac910-default-rtdb.firebaseio.com/"
#define FIREBASE_API_KEY "AIzaSyAnanb7oZ0LmlQrjb31NJLIxLi_GKPjBB4"

void setup()
{
    // Start Serial Monitor
    Serial.begin(115200);
    delay(1000);

    // Connect to Wi-Fi
    Serial.print("Connecting to Wi-Fi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Test Firebase connection
    HTTPClient http;
    String url = String(FIREBASE_HOST) + "/test.json?auth=" + FIREBASE_API_KEY;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == 200)
    {
        Serial.println("Firebase connection successful!");
        Serial.println("Response: " + http.getString());
    }
    else
    {
        Serial.println("Failed to connect to Firebase.");
        Serial.println("HTTP Code: " + String(httpCode));
    }

    http.end();
}

void loop()
{
    // Keep the loop empty for this basic example
}
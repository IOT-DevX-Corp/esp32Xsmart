// ntp server for real time clock and wifi connection string
#include <WiFi.h>
#include "time.h"
#include "esp_sntp.h"

// Replace with your network credentials
const char *ssid = "Hydra_Wlan$0";
const char *password = "Scienhac01";

// NTP server settings
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";

// GMT offset for India is +5:30 = 19800 seconds
const long gmtOffset_sec = 19800;
// India does not use daylight saving time
const int daylightOffset_sec = 0;

const char *time_zone = "IST-5:30"; // optional: tz string (not always required)

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

void timeavailable(struct timeval *t)
{
    Serial.println("Got time adjustment from NTP!");
    printLocalTime();
}

void setup()
{
    Serial.begin(115200);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected.");

    // Set notification callback function
    sntp_set_time_sync_notification_cb(timeavailable);

    // Set timezone and NTP servers
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

    // Or use the more advanced TZ string with configTzTime (uncomment below if needed)
    // configTzTime(time_zone, ntpServer1, ntpServer2);
}

void loop()
{
    delay(1000);
    printLocalTime();
}
#include <Arduino.h>

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

// Initialize buzzer
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
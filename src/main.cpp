#include <Arduino.h>

void setup()
{
    Serial.begin(115200);
    delay(1500);

    Serial.println();
    Serial.println("ESP32-S3 clock started");
    Serial.printf("Flash: %u MB\n",
                  ESP.getFlashChipSize() / 1024 / 1024);
    Serial.printf("PSRAM: %u MB\n",
                  ESP.getPsramSize() / 1024 / 1024);
}

void loop()
{
    Serial.println("ESP32-S3 працює");
    delay(2000);
}
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>
#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <U8g2_for_Adafruit_GFX.h>

#include "secrets.h"

const uint8_t TFT_CS   = 8;
const uint8_t TFT_DC   = 10;
const uint8_t TFT_RST  = 9;
const uint8_t TFT_MOSI = 11;
const uint8_t TFT_SCLK = 12;
const uint8_t BME_SDA  = 5;
const uint8_t BME_SCL  = 4;
const uint8_t LDR_PIN  = 6;
const uint8_t BACKLIGHT_PIN     = 7;
const uint8_t BACKLIGHT_CHANNEL = 0;

constexpr int LIGHT_DARK   = 1500;
constexpr int LIGHT_BRIGHT = 3600;
constexpr uint8_t MIN_BRIGHTNESS = 46;
constexpr uint8_t MAX_BRIGHTNESS = 255;

constexpr char TIME_ZONE[] = "EET-2EEST,M3.5.0/3,M10.5.0/4";

Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
U8G2_FOR_ADAFRUIT_GFX ukrainianFont;
Adafruit_BME280 bme;
bool bmeReady = false;

const char *const WEEKDAYS[] = {
    "Неділя",
    "Понеділок",
    "Вівторок",
    "Середа",
    "Четвер",
    "П'ятниця",
    "Субота"
};

int previousSecond = -1;

void drawCenteredText(const char *text, int16_t y, uint8_t size, uint16_t color,
                      uint16_t background = ST77XX_WHITE) {
    int16_t  x1;
    int16_t  y1;
    uint16_t width;
    uint16_t height;

    display.setTextSize(size);
    display.setTextColor(color, background);

    display.getTextBounds(text, 0, y, &x1, &y1, &width, &height);

    const int16_t x = (display.width() - static_cast<int16_t>(width)) / 2;

    display.setCursor(x, y);
    display.print(text);
}

void drawCenteredUkrainianText(const char *text, int16_t baselineY, uint16_t color,
                               uint16_t background = ST77XX_WHITE) {
    ukrainianFont.setForegroundColor(color);
    ukrainianFont.setBackgroundColor(background);

    const int16_t width = ukrainianFont.getUTF8Width(text);
    const int16_t x     = (display.width() - width) / 2;

    ukrainianFont.setCursor(x, baselineY);
    ukrainianFont.print(text);
}

bool connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    display.fillScreen(ST77XX_WHITE);
    drawCenteredText("CONNECTING", 130, 2, ST77XX_BLUE);

    const uint32_t startedAt = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 20000) {
        delay(250);
    }

    return WiFi.status() == WL_CONNECTED;
}

void drawClockFace() {
    display.fillScreen(ST77XX_WHITE);

    display.drawRoundRect(3, 3, display.width() - 6, display.height() - 6, 14, ST77XX_RED);

    display.drawRoundRect(4, 4, display.width() - 8, display.height() - 8, 13, ST77XX_RED);

}

void updateClock() {
    static int  previousMinute     = -1;
    static int  previousSecond     = -1;
    static int  previousDay        = -1;
    static bool timeErrorDisplayed = false;

    tm timeInfo{};

    if (!getLocalTime(&timeInfo, 100)) {
        if (!timeErrorDisplayed) {
            drawCenteredText("NO TIME", 140, 3, ST77XX_RED);

            timeErrorDisplayed = true;
        }

        return;
    }

    timeErrorDisplayed = false;

    if (timeInfo.tm_min != previousMinute) {
        previousMinute = timeInfo.tm_min;

        char timeText[6];
        strftime(timeText, sizeof(timeText), "%H:%M", &timeInfo);

        drawCenteredText(timeText, 50, 6, ST77XX_BLACK);
    }

    if (timeInfo.tm_sec != previousSecond) {
        previousSecond = timeInfo.tm_sec;

        char secondsText[3];
        strftime(secondsText, sizeof(secondsText), "%S", &timeInfo);

        drawCenteredText(secondsText, 110, 3, ST77XX_BLUE);
    }

    if (timeInfo.tm_yday != previousDay) {
        previousDay = timeInfo.tm_yday;

        char dateText[16];

        strftime(dateText, sizeof(dateText), "%d.%m.%Y", &timeInfo);

        display.fillRect(10, 170, display.width() - 20, 75, ST77XX_WHITE);

        drawCenteredText(dateText, 175, 2, ST77XX_BLACK);

        drawCenteredUkrainianText(WEEKDAYS[timeInfo.tm_wday], 232, ST77XX_MAGENTA);
    }
}

void updateRoomClimate() {
    if (!bmeReady) {
        drawCenteredText("BME280 ERROR", 255, 1, ST77XX_RED);
        return;
    }

    const float temperature = bme.readTemperature();
    const float humidity    = bme.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
        return;
    }

    char climateText[32];
    snprintf(climateText, sizeof(climateText), "%.1f °C   %.0f %%", temperature, humidity);

    display.fillRect(10, 247, display.width() - 20, 25, ST77XX_WHITE);
    drawCenteredUkrainianText(climateText, 266, ST77XX_GREEN);
}

void setBacklightBrightness(uint8_t brightness) {
    // This display module's LED input makes the complete control chain non-inverting.
    ledcWrite(BACKLIGHT_CHANNEL, brightness);
}

void updateBacklight() {
    static float filteredLightLevel = -1.0F;

    const int rawLightLevel = analogRead(LDR_PIN);

    if (filteredLightLevel < 0.0F) {
        filteredLightLevel = static_cast<float>(rawLightLevel);
    } else {
        filteredLightLevel = filteredLightLevel * 0.8F + static_cast<float>(rawLightLevel) * 0.2F;
    }

    const int lightLevel = constrain(
        static_cast<int>(filteredLightLevel),
        LIGHT_DARK,
        LIGHT_BRIGHT
    );

    const uint8_t brightness = static_cast<uint8_t>(map(
        lightLevel,
        LIGHT_DARK,
        LIGHT_BRIGHT,
        MIN_BRIGHTNESS,
        MAX_BRIGHTNESS
    ));

    setBacklightBrightness(brightness);

    Serial.print("Light: ");
    Serial.print(rawLightLevel);
    Serial.print(" | Brightness: ");
    Serial.print((static_cast<unsigned int>(brightness) * 100U) / 255U);
    Serial.println("%");
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(LDR_PIN, INPUT);
    analogReadResolution(12);

    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, HIGH);
    ledcSetup(BACKLIGHT_CHANNEL, 5000, 8);
    ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_CHANNEL);
    setBacklightBrightness(180);

    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

    display.init(240, 320);
    display.setRotation(0);

    Wire.begin(BME_SDA, BME_SCL);
    bmeReady = bme.begin(0x76, &Wire);

    if (!bmeReady) {
        bmeReady = bme.begin(0x77, &Wire);
    }

    if (!bmeReady) {
        Serial.println("BME280 not found at 0x76 or 0x77");
    }

    ukrainianFont.begin(display);
    ukrainianFont.setFontMode(1);
    ukrainianFont.setFontDirection(0);
    ukrainianFont.setFont(u8g2_font_unifont_t_cyrillic);

    if (!connectWiFi()) {
        display.fillScreen(ST77XX_WHITE);
        drawCenteredText("WIFI ERROR", 140, 3, ST77XX_RED);
        Serial.println("Wi-Fi connection failed");
        return;
    }

    Serial.print("Connected, IP: ");
    Serial.println(WiFi.localIP());

    configTzTime(TIME_ZONE, "pool.ntp.org", "time.google.com", "time.cloudflare.com");

    tm timeInfo{};

    if (!getLocalTime(&timeInfo, 15000)) {
        display.fillScreen(ST77XX_WHITE);
        drawCenteredText("NTP ERROR", 140, 3, ST77XX_RED);
        Serial.println("NTP synchronization failed");
        return;
    }

    drawClockFace();
    updateClock();
    updateRoomClimate();
    updateBacklight();
}

void loop() {
    static unsigned long previousClockUpdate   = 0;
    static unsigned long previousClimateUpdate = 0;
    static unsigned long previousLightUpdate   = 0;

    constexpr unsigned long CLOCK_INTERVAL_MS   = 100;
    constexpr unsigned long CLIMATE_INTERVAL_MS = 5000;
    constexpr unsigned long LIGHT_INTERVAL_MS   = 500;

    const unsigned long currentMillis = millis();

    if (currentMillis - previousClockUpdate >= CLOCK_INTERVAL_MS) {
        previousClockUpdate = currentMillis;
        updateClock();
    }

    if (currentMillis - previousClimateUpdate >= CLIMATE_INTERVAL_MS) {
        previousClimateUpdate = currentMillis;
        updateRoomClimate();
    }

    if (currentMillis - previousLightUpdate >= LIGHT_INTERVAL_MS) {
        previousLightUpdate = currentMillis;
        updateBacklight();
    }
}

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <driver/i2s.h>
#include "fish_bitmaps.h"

// ---- OLED ----
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_I2C_ADDR 0x3C

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---- I2S mic pins ----
#define I2S_SCK_PIN 14
#define I2S_WS_PIN  15
#define I2S_SD_PIN  32

// ---- Audio ----
#define SAMPLE_RATE       16000
#define MIC_CHUNK_SAMPLES 256

// ---- Tuning ----
// Raise SOUND_THRESHOLD if background noise triggers the fish.
// Lower it if the fish don't react to normal speech.
// Serial monitor prints amplitude on each trigger to help calibrate.
#define SOUND_THRESHOLD  2500

// ---- Frame timing ----
#define IDLE_FRAME_MS     180   // idle swimming speed
#define DASH_FRAME_MS      45   // frantic dash speed
#define HIDE_FRAME_MS     220   // slow creep into hiding

// How long the fish dash before transitioning to hiding
#define DASH_DURATION_MS 2000

// How long the fish hide before returning to idle
#define HIDE_DURATION_MS 3000

// ---- Frame ranges (0-indexed) ----
#define IDLE_FIRST   0
#define IDLE_LAST    9   // frames 1-10
#define DASH_FIRST  10
#define DASH_LAST   19   // frames 11-20
#define HIDE_FIRST  20
#define HIDE_LAST   24   // frames 21-25

// ---- State machine ----
enum FishState { IDLE, DASHING, HIDING };
FishState fishState   = IDLE;

int  currentFrame     = IDLE_FIRST;
unsigned long lastFrameMs   = 0;
unsigned long stateStartMs  = 0;

// Read a frame from PROGMEM pointer table safely
const unsigned char* getFrame(int index)
{
    return (const unsigned char*)pgm_read_ptr(&fishFrames[index]);
}

void showFrame(int index)
{
    display.clearDisplay();
    display.drawBitmap(0, 0, getFrame(index), 128, 64, SH110X_WHITE);
    display.display();
}

void enterState(FishState newState)
{
    fishState    = newState;
    stateStartMs = millis();

    switch (newState)
    {
        case IDLE:
            currentFrame = IDLE_LAST;
            break;
        case DASHING:
            currentFrame = DASH_LAST;
            Serial.println(">> DASHING");
            break;
        case HIDING:
            currentFrame = HIDE_LAST;
            Serial.println(">> HIDING");
            break;
    }

    showFrame(currentFrame);
    lastFrameMs = millis();
}

// ---- I2S setup ----
void setupI2S()
{
    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 64,
        .use_apll = false
    };

    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_SCK_PIN,
        .ws_io_num    = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_SD_PIN
    };

    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);
}

int32_t readPeakAmplitude()
{
    int16_t buf[MIC_CHUNK_SAMPLES];
    size_t bytesRead = 0;

    i2s_read(I2S_NUM_0, buf, sizeof(buf), &bytesRead, portMAX_DELAY);

    int32_t peak = 0;
    size_t samplesRead = bytesRead / 2;
    for (size_t i = 0; i < samplesRead; i++)
    {
        int32_t val = abs((int32_t)buf[i]);
        if (val > peak) peak = val;
    }
    return peak;
}

void setup()
{
    Serial.begin(115200);

    Wire.begin();
    if (!display.begin(OLED_I2C_ADDR, true))
    {
        while (true) delay(1000);
    }

    setupI2S();
    enterState(IDLE);
}

void loop()
{
    unsigned long now = millis();

    // ---- Read mic ----
    int32_t amplitude = readPeakAmplitude();

    // ---- State transitions ----
    switch (fishState)
    {
        case IDLE:
            // Sound triggers a dash
            if (amplitude > SOUND_THRESHOLD)
            {
                Serial.print("Sound! amplitude: ");
                Serial.println(amplitude);
                enterState(DASHING);
                return;
            }
            break;

        case DASHING:
            // After DASH_DURATION_MS, transition to hiding
            if (now - stateStartMs >= DASH_DURATION_MS)
            {
                enterState(HIDING);
                return;
            }
            // New loud sound while dashing: reset dash timer so they
            // keep dashing as long as there's noise
            if (amplitude > SOUND_THRESHOLD)
            {
                stateStartMs = now;
            }
            break;

        case HIDING:
            // After HIDE_DURATION_MS, return to idle
            if (now - stateStartMs >= HIDE_DURATION_MS)
            {
                enterState(IDLE);
                return;
            }
            // Sound while hiding: go back to dashing
            if (amplitude > SOUND_THRESHOLD)
            {
                Serial.println("Sound while hiding! Back to dashing.");
                enterState(DASHING);
                return;
            }
            break;
    }

    // ---- Advance animation frame ----
    unsigned long frameDelay;
    int firstFrame, lastFrame;

    switch (fishState)
    {
        case IDLE:
            frameDelay = IDLE_FRAME_MS;
            firstFrame = IDLE_FIRST;
            lastFrame  = IDLE_LAST;
            break;
        case DASHING:
            frameDelay = DASH_FRAME_MS;
            firstFrame = DASH_FIRST;
            lastFrame  = DASH_LAST;
            break;
        case HIDING:
            frameDelay = HIDE_FRAME_MS;
            firstFrame = HIDE_FIRST;
            lastFrame  = HIDE_LAST;
            break;
        default:
            return;
    }

    if (now - lastFrameMs >= frameDelay)
    {
        currentFrame--;
        if (currentFrame < firstFrame)
            currentFrame = lastFrame;

        showFrame(currentFrame);
        lastFrameMs = now;
    }
}

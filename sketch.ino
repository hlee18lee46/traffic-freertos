#include <Arduino.h>

extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/queue.h"
  #include "freertos/semphr.h"
}

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// OLED
// =====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// =====================================================
// Pins
// =====================================================

#define RED_LED       25
#define YELLOW_LED    26
#define GREEN_LED     27

#define WALK_LED      13
#define STOP_LED      12

#define BUTTON_PIN    33

// =====================================================
// FreeRTOS Objects
// =====================================================

QueueHandle_t crossingQueue;
SemaphoreHandle_t displayMutex;

// =====================================================
// Traffic States
// =====================================================

enum TrafficState {
  STATE_GREEN,
  STATE_YELLOW,
  STATE_WALK,
  STATE_WAIT
};

volatile TrafficState currentState = STATE_GREEN;

// =====================================================
// OLED Helper
// =====================================================

void updateDisplay(
  const char *line1,
  const char *line2
) {
  xSemaphoreTake(
    displayMutex,
    portMAX_DELAY
  );

  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println(line1);

  display.setCursor(0, 32);
  display.println(line2);

  display.display();

  xSemaphoreGive(displayMutex);
}

// =====================================================
// Pedestrian Task
// PRODUCER
// =====================================================

void pedestrianTask(void *pvParameters)
{
  bool request = true;

  while (true) {

    if (digitalRead(BUTTON_PIN) == LOW) {

      Serial.println(
        "[BUTTON] Crossing Requested"
      );

      xQueueSend(
        crossingQueue,
        &request,
        0
      );

      // debounce
      vTaskDelay(
        pdMS_TO_TICKS(500)
      );
    }

    vTaskDelay(
      pdMS_TO_TICKS(50)
    );
  }
}

// =====================================================
// Traffic Task
// CONSUMER
// =====================================================

void trafficTask(void *pvParameters)
{
  bool request;

  while (true) {

    // -------------------------------------------------
    // NORMAL TRAFFIC
    // -------------------------------------------------

    currentState = STATE_GREEN;

    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, LOW);

    digitalWrite(WALK_LED, LOW);
    digitalWrite(STOP_LED, HIGH);

    updateDisplay(
      "PED",
      "DONT WALK"
    );

    Serial.println(
      "[TRAFFIC] Cars Green"
    );

    // Wait for a crossing request
    xQueueReceive(
      crossingQueue,
      &request,
      portMAX_DELAY
    );

    // -------------------------------------------------
    // YELLOW
    // -------------------------------------------------

    currentState = STATE_YELLOW;

    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);

    updateDisplay(
      "WAIT",
      "YELLOW"
    );

    Serial.println(
      "[TRAFFIC] Yellow"
    );

    vTaskDelay(
      pdMS_TO_TICKS(3000)
    );

    // -------------------------------------------------
    // WALK
    // -------------------------------------------------

    currentState = STATE_WALK;

    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, HIGH);

    digitalWrite(STOP_LED, LOW);
    digitalWrite(WALK_LED, HIGH);

    updateDisplay(
      "PED",
      "WALK"
    );

    Serial.println(
      "[TRAFFIC] Walk"
    );

    vTaskDelay(
      pdMS_TO_TICKS(7000)
    );

    // -------------------------------------------------
    // CLEARANCE
    // -------------------------------------------------

    currentState = STATE_WAIT;

    digitalWrite(WALK_LED, LOW);
    digitalWrite(STOP_LED, HIGH);

    updateDisplay(
      "PED",
      "STOP"
    );

    Serial.println(
      "[TRAFFIC] Clearance"
    );

    vTaskDelay(
      pdMS_TO_TICKS(2000)
    );

    digitalWrite(RED_LED, LOW);
  }
}

// =====================================================
// Display Monitor Task
// =====================================================

void displayMonitorTask(
  void *pvParameters
)
{
  while (true) {

    xSemaphoreTake(
      displayMutex,
      portMAX_DELAY
    );

    Serial.print(
      "[DISPLAY] State = "
    );

    Serial.println(
      currentState
    );

    xSemaphoreGive(
      displayMutex
    );

    vTaskDelay(
      pdMS_TO_TICKS(2000)
    );
  }
}

// =====================================================
// Logger Task
// =====================================================

void loggerTask(void *pvParameters)
{
  while (true) {

    switch (currentState) {

      case STATE_GREEN:
        Serial.println(
          "[LOGGER] GREEN"
        );
        break;

      case STATE_YELLOW:
        Serial.println(
          "[LOGGER] YELLOW"
        );
        break;

      case STATE_WALK:
        Serial.println(
          "[LOGGER] WALK"
        );
        break;

      case STATE_WAIT:
        Serial.println(
          "[LOGGER] WAIT"
        );
        break;
    }

    vTaskDelay(
      pdMS_TO_TICKS(1000)
    );
  }
}

// =====================================================
// Setup
// =====================================================

void setup()
{
  Serial.begin(115200);

  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  pinMode(WALK_LED, OUTPUT);
  pinMode(STOP_LED, OUTPUT);

  pinMode(
    BUTTON_PIN,
    INPUT_PULLUP
  );

  Wire.begin(21, 22);

  display.begin(
    SSD1306_SWITCHCAPVCC,
    0x3C
  );

  display.clearDisplay();
  display.display();

  crossingQueue =
    xQueueCreate(
      5,
      sizeof(bool)
    );

  displayMutex =
    xSemaphoreCreateMutex();

  xTaskCreate(
    pedestrianTask,
    "PedestrianTask",
    2048,
    NULL,
    4,
    NULL
  );

  xTaskCreate(
    trafficTask,
    "TrafficTask",
    4096,
    NULL,
    3,
    NULL
  );

  xTaskCreate(
    displayMonitorTask,
    "DisplayTask",
    2048,
    NULL,
    2,
    NULL
  );

  xTaskCreate(
    loggerTask,
    "LoggerTask",
    2048,
    NULL,
    1,
    NULL
  );
}

// =====================================================
// Arduino Loop
// =====================================================

void loop()
{
  vTaskDelay(
    pdMS_TO_TICKS(1000)
  );
}

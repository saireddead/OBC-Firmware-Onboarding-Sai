#include "thermal_mgr.h"
#include "errors.h"
#include "lm75bd.h"
#include "console.h"

#include <FreeRTOS.h>
#include <os_task.h>
#include <os_queue.h>

#include <string.h>
#include <stdlib.h>

#define THERMAL_MGR_STACK_SIZE 256U

static TaskHandle_t thermalMgrTaskHandle;
static StaticTask_t thermalMgrTaskBuffer;
static StackType_t thermalMgrTaskStack[THERMAL_MGR_STACK_SIZE];

#define THERMAL_MGR_QUEUE_LENGTH 10U
#define THERMAL_MGR_QUEUE_ITEM_SIZE sizeof(thermal_mgr_event_t)

static QueueHandle_t thermalMgrQueueHandle;
static StaticQueue_t thermalMgrQueueBuffer;
static uint8_t thermalMgrQueueStorageArea[THERMAL_MGR_QUEUE_LENGTH * THERMAL_MGR_QUEUE_ITEM_SIZE];

static void thermalMgr(void *pvParameters);

void initThermalSystemManager(lm75bd_config_t *config) {
  memset(&thermalMgrTaskBuffer, 0, sizeof(thermalMgrTaskBuffer));
  memset(thermalMgrTaskStack, 0, sizeof(thermalMgrTaskStack));
  
  thermalMgrTaskHandle = xTaskCreateStatic(
    thermalMgr, "thermalMgr", THERMAL_MGR_STACK_SIZE,
    config, 1, thermalMgrTaskStack, &thermalMgrTaskBuffer);

  memset(&thermalMgrQueueBuffer, 0, sizeof(thermalMgrQueueBuffer));
  memset(thermalMgrQueueStorageArea, 0, sizeof(thermalMgrQueueStorageArea));

  thermalMgrQueueHandle = xQueueCreateStatic(
    THERMAL_MGR_QUEUE_LENGTH, THERMAL_MGR_QUEUE_ITEM_SIZE,
    thermalMgrQueueStorageArea, &thermalMgrQueueBuffer);

}

error_code_t thermalMgrSendEvent(thermal_mgr_event_t *event) {
  /* Send an event to the thermal manager queue */
  BaseType_t errCode = 
  xQueueSend
  (
    thermalMgrQueueHandle,
    event,
    portMAX_DELAY
  );

  if(errCode == pdPASS)
  {
    return ERR_CODE_SUCCESS;
  }
  else
  {
    return ERR_CODE_QUEUE_FULL;
  }
}

void osHandlerLM75BD(void) //OS handler ISR
{
  thermal_mgr_event_t osReset; //Creates thermal manager event
  osReset.type = THERMAL_MGR_EVENT_OS_RESET; //Defines type as OS reset
  xQueueSendToFrontFromISR //Sends OS reset event to the front of the thermal manager queue
  (
    thermalMgrQueueHandle,
    &osReset,
    NULL
  );
}

static void thermalMgr(void *pvParameters) //Thermal manager task
{
  lm75bd_config_t thermal_config = *(lm75bd_config_t*)(pvParameters); //Casting LM75BD config
  uint8_t devAddr = thermal_config.devAddr; //Creating variable for dev address
  thermal_mgr_event_t *event = malloc(sizeof(thermal_mgr_event_t)); //Creating event buffer to receive from queue
  float osThreshold = thermal_config.overTempThresholdCelsius; //Creating variable for overtemperature threshold 
  float hysThreshold = thermal_config.hysteresisThresholdCelsius; //Creating variable for hysteresis threshold
  error_code_t errCode; //Defining errcode 
  while (1) 
  {
    BaseType_t queueErrCode = //Receiving thermal manager event from queue
    xQueueReceive
    (
      thermalMgrQueueHandle,
      event,
      portMAX_DELAY
    );
    if(queueErrCode != pdPASS) //Checks if xQueueReceive timed out
    {
    }
    else if(event->type == THERMAL_MGR_EVENT_MEASURE_TEMP_CMD) //Checks if received event is for telemetry logging
    {
      float* temp = (float*)(malloc(sizeof(float))); //Creating pointer to store temp
      errCode = readTempLM75BD(devAddr,temp); //Reading temperature from LM75BD
      if(errCode == ERR_CODE_SUCCESS) //Checks if temperature was read successfully
      {
        addTemperatureTelemetry(*temp); //Logs temperature telemetry data in console
      }
    }
    else if(event->type == THERMAL_MGR_EVENT_OS_RESET) //Checks if received event is for OS output reset
    {
      float* temp = (float*)(malloc(sizeof(float))); //Creating pointer to store temp
      errCode = readTempLM75BD(devAddr,temp); //Reading temperature from LM75BD and reset OS output
      if(errCode != ERR_CODE_SUCCESS); //Checks if temperature was read successfully
      else if(*temp >= osThreshold) //Checks if temperature exceeds overtemperature threshold
      {
        overTemperatureDetected(); //Logs overtemperature event in console
      }
      else if(*temp <= hysThreshold) //Checks if temperature is below hysteresis threshold
      {
        safeOperatingConditions(); //Logs return to safe operating conditions event in console
      }
    }
  }
}
void addTemperatureTelemetry(float tempC) {
  printConsole("Temperature telemetry: %f deg C\n", tempC);
}

void overTemperatureDetected(void) {
  printConsole("Over temperature detected!\n");
}

void safeOperatingConditions(void) { 
  printConsole("Returned to safe operating conditions!\n");
}

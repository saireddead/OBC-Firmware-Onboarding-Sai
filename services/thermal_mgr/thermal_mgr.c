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

void osHandlerLM75BD(void) 
{
  thermal_mgr_event_t osReset;
  osReset.type = THERMAL_MGR_EVENT_OS_RESET;
  xQueueSendToFrontFromISR
  (
    thermalMgrQueueHandle,
    &osReset,
    NULL
  );
}

static void thermalMgr(void *pvParameters) 
{
  lm75bd_config_t thermal_config = *(lm75bd_config_t*)(pvParameters);
  uint8_t devAddr = thermal_config.devAddr;
  thermal_mgr_event_t *event = malloc(sizeof(thermal_mgr_event_t));
  float osThreshold = thermal_config.overTempThresholdCelsius;
  float hysThreshold = thermal_config.hysteresisThresholdCelsius;
  error_code_t errCode;
  while (1) 
  {
    BaseType_t queueErrCode = 
    xQueueReceive
    (
      thermalMgrQueueHandle,
      event,
      portMAX_DELAY
    );
    if(queueErrCode != pdPASS)
    {
    }
    else if(event->type == THERMAL_MGR_EVENT_MEASURE_TEMP_CMD)
    {
      float* temp = (float*)(malloc(sizeof(float)));
      errCode = readTempLM75BD(devAddr,temp);
      if(errCode == ERR_CODE_SUCCESS)
      {
        addTemperatureTelemetry(*temp);
      }
    }
    else if(event->type == THERMAL_MGR_EVENT_OS_RESET)
    {
      float* temp = (float*)(malloc(sizeof(float)));
      errCode = readTempLM75BD(devAddr,temp);
      if(errCode != ERR_CODE_SUCCESS);
      else if(*temp >= osThreshold)
      {
        overTemperatureDetected();
      }
      else if(*temp <= hysThreshold)
      {
        safeOperatingConditions();
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

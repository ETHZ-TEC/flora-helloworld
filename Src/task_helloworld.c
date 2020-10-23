/**
  ******************************************************************************
  * Flora
  ******************************************************************************
  * @file   task_helloworld.c
  * @brief  Hello World task
  *
  *
  ******************************************************************************
  */

#include "main.h"


/* Macros */

#define NOTIFY_BS_TASK(val)           xTaskNotify(xTaskHandle_basestation, val, eSetValueWithOverwrite);
#define NOTIFY_BS_TASK_FROM_ISR(val)  xTaskNotifyFromISR(xTaskHandle_basestation, val, eSetValueWithOverwrite, NULL)


/* Variables */

extern TaskHandle_t  xTaskHandle_helloworld;
extern TaskHandle_t  xTaskHandle_bolt;
extern TaskHandle_t  xTaskHandle_timesync;


/* Functions */

void periodic_cb(void)
{
  update_opmode(OP_MODE_EVT_WAKEUP);
  vTaskNotifyGiveFromISR(xTaskHandle_helloworld, 0);
}


void task_helloworld(void const * argument)
{
  LOG_VERBOSE("hello world task has started");

  // start the task in 1s
  lptimer_set(lptimer_now() + LPTIMER_S_TO_TICKS(1), periodic_cb);

  for (;;)
  {
    // wait until task gets unblocked
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    LOG_VERBOSE("hello world! (%lu)", (uint32_t)(get_time(0) / 1000000));
    led_on(LED_SYSTEM);
    vTaskDelay(pdMS_TO_TICKS(100));
    led_off(LED_SYSTEM);

    // set a timer to trigger the next wakeup
    lptimer_set(lptimer_now() + LPTIMER_S_TO_TICKS(WAKEUP_PERIOD_S), periodic_cb);

    // poll the BOLT and debug tasks
    xTaskNotifyGive(xTaskHandle_bolt);
    xTaskNotifyGive(xTaskHandle_timesync);

    update_opmode(OP_MODE_EVT_DONE);      // signal the LPM state machine to go to STOP mode
  }
}


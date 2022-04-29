/*
 * Copyright (c) 2020 - 2022, ETH Zurich, Computer Engineering Group (TEC)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
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
  lpm_update_opmode(OP_MODE_EVT_WAKEUP);
  vTaskNotifyGiveFromISR(xTaskHandle_helloworld, 0);
}


void task_helloworld(void const * argument)
{
  LOG_VERBOSE("hello world task has started");

  /* start the task in 1s */
  lptimer_set(lptimer_now() + LPTIMER_S_TO_TICKS(1), periodic_cb);

  for (;;)
  {
    /* wait until task gets unblocked */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    uint32_t t_now = get_time(0) / 1000000;
    LOG_VERBOSE("hello world! %lu", t_now);
    led_on(LED_SYSTEM);
    vTaskDelay(pdMS_TO_TICKS(100));
    led_off(LED_SYSTEM);

#if BASEBOARD
    /* execute pending baseboard commands */
    process_scheduled_commands();
#endif

    /* set a timer to trigger the next wakeup */
    lptimer_set(lptimer_now() + LPTIMER_S_TO_TICKS(WAKEUP_PERIOD_S), periodic_cb);

    /* poll the BOLT and debug tasks */
    xTaskNotifyGive(xTaskHandle_bolt);
    xTaskNotifyGive(xTaskHandle_timesync);
    /* note: task will be interrupted at this point and resumes once the bolt and timesync tasks yield */

    /* signal the LPM state machine to go to STOP mode */
    lpm_update_opmode(OP_MODE_EVT_DONE);
  }
}


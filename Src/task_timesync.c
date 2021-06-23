/*
 * Copyright (c) 2020 - 2021, ETH Zurich, Computer Engineering Group (TEC)
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

/*
 * handles the time synchronization
 */

#include "main.h"


/* Global variables ----------------------------------------------------------*/

extern TaskHandle_t      xTaskHandle_timesync;
extern TaskHandle_t      xTaskHandle_helloworld;


/* Private define ------------------------------------------------------------*/


/* Private variables and functions -------------------------------------------*/

static uint64_t unix_timestamp        = 0;        /* UNIX timestamp of the last sync point, in us */
static uint64_t local_timestamp       = 0;        /* local timestamp of the last sync point, in timer ticks (source node: lptimer, base station: hs timer) */
static uint64_t captured_timestamp    = 0;        /* captured timestamp of the last time request (COM_TREQ rising edge) in lptimer ticks -> only used on source nodes */
static int32_t  average_drift         = 0;        /* average drift of the local timer towards the time master, in ppm */
static bool     timestamp_updated     = false;
static bool     timestamp_requested   = false;


/* Functions -----------------------------------------------------------------*/

static void update_time(void)
{
  static uint64_t prev_local_timestamp = 0;
  static uint64_t prev_unix_timestamp  = 0;

  if (timestamp_updated) {
    /* calculate the drift */
    if (prev_unix_timestamp) {
      int64_t master_ts_diff_us = (unix_timestamp - prev_unix_timestamp);
      int64_t local_ts_diff_us = ((uint64_t)(local_timestamp - prev_local_timestamp) * 1000000 / LPTIMER_SECOND);
      int32_t drift = (int64_t)(local_ts_diff_us - master_ts_diff_us) * 1000000 / master_ts_diff_us;
      /* drift has to be within a certain range */
      if (drift < TIMESTAMP_MAX_DRIFT && drift > -TIMESTAMP_MAX_DRIFT) {
        if (drift > TIMESTAMP_TYPICAL_DRIFT || drift < -TIMESTAMP_TYPICAL_DRIFT) {
          LOG_WARNING("drift is larger than usual");
        }
        if (average_drift == 0) {
          average_drift = drift;
        } else {
          average_drift = (average_drift + drift) / 2;
        }
        /* make sure the drift does not exceed the maximum allowed value */
        if (average_drift > TIMESTAMP_MAX_DRIFT) {
          average_drift = TIMESTAMP_MAX_DRIFT;
        } else if (average_drift < -TIMESTAMP_MAX_DRIFT) {
          average_drift = -TIMESTAMP_MAX_DRIFT;
        }
        /* note: a negative drift means the local time runs slower than the master clock */
        LOG_VERBOSE("current drift: %ldppm   average drift: %ldppm", drift, average_drift);

      } else {
        LOG_WARNING("drift is too large (%ldppm)", drift);
      }
    }
    LOG_VERBOSE("time updated");

    prev_local_timestamp = local_timestamp;
    prev_unix_timestamp  = unix_timestamp;
  }
  timestamp_updated   = false;
  timestamp_requested = false;
}


/* set the time based on a given UNIX timestamp and hstimer ticks timestamp pair */
void set_time(uint64_t unix_time_us)
{
  if (timestamp_requested) {
    unix_timestamp      = unix_time_us;
    local_timestamp     = captured_timestamp;
    timestamp_requested = false;
    timestamp_updated   = true;

    if (!IS_INTERRUPT()) {
      xTaskNotifyGive(xTaskHandle_timesync);
    }
  } else {
    LOG_WARNING("time not updated (no captured timestamp)");
  }
}


uint64_t get_time(uint64_t at_time)
{
  if (at_time == 0) {
    at_time = lptimer_now();
  }
  return unix_timestamp + (((int64_t)at_time - (int64_t)local_timestamp) * (1000000LL - average_drift) / LPTIMER_SECOND);
}


void GPIO_PIN_3_Callback(void)
{
  if (!timestamp_requested) {
    /* only overwrite if there is not already a pending timestamp request */
    captured_timestamp  = lptimer_now() - 1;   /* subtract wakeup + ISR + function call delays (measured to ~20us) */
    timestamp_requested = true;
  }
  lpm_update_opmode(OP_MODE_EVT_WAKEUP);
  LOG_VERBOSE("timestamp captured (%llu)", captured_timestamp);
  vTaskNotifyGiveFromISR(xTaskHandle_helloworld, 0);
}


void task_timesync(void const * argument)
{
  LOG_VERBOSE("timesync task started");

  /* Infinite loop */
  for (;;)
  {
    /* wait for notification token (= explicitly granted permission to run) */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    update_time();

#if BASEBOARD_TREQ_WATCHDOG && BASEBOARD
    static uint64_t last_treq = 0;
    if (captured_timestamp > last_treq) {
      last_treq = captured_timestamp;
    }
    /* only use time request watchdog when baseboard is enabled */
    if (PIN_STATE(BASEBOARD_ENABLE)) {
      bool powercycle = false;
      /* check when was the last time we got a time request */
      if (LPTIMER_TICKS_TO_S(lptimer_now() - last_treq) > BASEBOARD_TREQ_WATCHDOG) {
        last_treq = lptimer_now();
        powercycle = true;
      }
      if (powercycle) {
        /* power cycle the baseboard */
        LOG_WARNING("power-cycling baseboard (TREQ watchdog)");
        PIN_CLR(BASEBOARD_ENABLE);
        /* enable pin must be kept low for ~1s -> schedule pin release */
        if (!schedule_command((get_time(0) / 1000000) + 2, CMD_SX1262_BASEBOARD_ENABLE, 0)) {
          /* we must wait and release the reset here */
          LOG_WARNING("failed to schedule baseboard enable");
          delay_us(60000);
          PIN_SET(BASEBOARD_ENABLE);
        }
      }
    } else {
      last_treq = lptimer_now();
    }
#endif /* BASEBOARD_TREQ_WATCHDOG */

    //LOG_VERBOSE("timesync executed");
  }
}


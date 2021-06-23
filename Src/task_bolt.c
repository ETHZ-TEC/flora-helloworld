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
 * reads messages from BOLT
 */

#include "main.h"


#if BOLT_ENABLE

/* Global variables ----------------------------------------------------------*/


/* Private define ------------------------------------------------------------*/


/* Private variables and functions -------------------------------------------*/


/* Functions -----------------------------------------------------------------*/

void task_bolt(void const * argument)
{
  LOG_VERBOSE("bolt task started");

  /* empty the BOLT queue */
  bolt_flush();

  /* Infinite loop */
  for (;;)
  {
    /* wait for notification token (= explicitly granted permission to run) */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    /* read from BOLT */
    static uint8_t  bolt_read_buffer[BOLT_MAX_MSG_LEN];
    uint32_t max_read_cnt = BOLT_MAX_READ_COUNT;
    /* only read as long as there is still space in the transmit queue */
    while (max_read_cnt && BOLT_DATA_AVAILABLE) {
      uint32_t len = bolt_read(bolt_read_buffer);
      if (!len) {
        LOG_ERROR("bolt read failed");
        break;
      }
      process_message((dpp_message_t*)bolt_read_buffer, true);
      max_read_cnt--;
    }
    if (max_read_cnt < BOLT_MAX_READ_COUNT) {
      LOG_VERBOSE("%lu msg read from BOLT", BOLT_MAX_READ_COUNT - max_read_cnt);
    }

    //LOG_VERBOSE("bolt task executed");
  }
}

#endif /* BOLT_ENABLE */

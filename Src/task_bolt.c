/*
 * task_bolt.c
 *
 * reads messages from BOLT
 *
 *  Created on: Aug 22, 2019
 *      Author: rdaforno
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

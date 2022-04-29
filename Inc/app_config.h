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
 * Flora "hello world" config
 */

#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H


/* --- adjustable parameters --- */

/* general */
#define NODE_ID                         2
#define BASEBOARD                       0               /* set to 1 if the comboard will be installed on a baseboard */
#define WAKEUP_PERIOD_S                 60              /* period at which the hello world task will run */
#define LOW_POWER_MODE                  LP_MODE_STOP2   /* low-power mode to use between rounds during periods of inactivity */
#define LPM_DISABLE_GPIO_CLOCKS         0               /* set to 1 to disable GPIO clocks in low-power mode (-> no GPIO tracing possible) */
#define BASEBOARD_TREQ_WATCHDOG         900             /* if != 0, the baseboard will be power-cycled if no time request has been received within the specified #seconds */

/* timesync */
#define TIMESTAMP_TYPICAL_DRIFT_PPM     40    /* typical drift +/- in ppm (if exceeded, a warning will be issued) */
#define TIMESTAMP_MAX_DRIFT_PPM         100   /* max. allowed drift in ppm (higher values will be capped) */

/* memory */
#define BOLT_TASK_STACK_SIZE            256   /* in # words of 4 bytes */
#define TIMESYNC_TASK_STACK_SIZE        256   /* in # words of 4 bytes */
#define HELLOWORLD_TASK_STACK_SIZE      256   /* in # words of 4 bytes */
#define COMMAND_QUEUE_SIZE              10    /* queue size for baseboard enable/disable commands */
#define BOLT_MAX_READ_COUNT             100   /* max. number of messages to read from BOLT at once */

/* Flora lib config */
#define HS_TIMER_COMPENSATE_DRIFT       0
#define HS_TIMER_INIT_FROM_RTC          0
#define LPTIMER_RESET_WDG_ON_OVF        1
#define LPTIMER_RESET_WDG_ON_EXP        0
#define LPTIMER_CHECK_EXP_TIME          1
#define UART_FIFO_BUFFER_SIZE           1           /* not used, set to 1 byte to reduce memory usage */
#define CLI_ENABLE                      0           /* command line interface */
#define SWO_ENABLE                      0
#define BOLT_ENABLE                     1
#define NVCFG_ENABLE                    1

/* logging */
#define LOG_ENABLE                      1
#define LOG_LEVEL                       LOG_LEVEL_VERBOSE
#define LOG_PRINT_IMMEDIATELY           1           /* if set to zero, the debug task is responsible for printing out the debug messages via UART */
#if BASEBOARD
  #define LOG_ADD_TIMESTAMP             0           /* don't print the timestamp on the baseboard */
  #define LOG_USE_COLORS                0
  #define LOG_LEVEL_ERROR_STR           "<3>"       /* use syslog severity level number instead of strings */
  #define LOG_LEVEL_WARNING_STR         "<4>"
  #define LOG_LEVEL_INFO_STR            "<6>"
  #define LOG_LEVEL_VERBOSE_STR         "<7>"
#endif /* BASEBOARD */

#define CPU_ON_IND()                    //PIN_SET(COM_PROG)
#define CPU_OFF_IND()                   //PIN_CLR(COM_PROG)
#define LPM_ON_IND()                    //PIN_CLR(COM_PROG)
#define LPM_OFF_IND()                   //PIN_SET(COM_PROG)


/* --- parameter checks --- */

#if BOLT_ENABLE && (BOLT_MAX_MSG_LEN < DPP_MSG_PKT_LEN)
#error "BOLT_MAX_MSG_LEN is too small"
#endif

#if BASEBOARD_TREQ_WATCHDOG > 0 && BASEBOARD_TREQ_WATCHDOG < 120
#error "BASEBOARD_TREQ_WATCHDOG must be >= 120"
#endif

#endif /* __APP_CONFIG_H */

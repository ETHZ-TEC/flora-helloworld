/**
  ******************************************************************************
  * Flora hello world - config
  ******************************************************************************
  * @file   app_config.h
  * @brief  application config file
  *
  *
  ******************************************************************************
  */

#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H


/* --- adjustable parameters --- */

/* general */
#define NODE_ID                         2
#define BASEBOARD                       1               /* set to 1 if the comboard will be installed on a baseboard */
#define WAKEUP_PERIOD_S                 60              /* period at which the hello world task will run */
#define LOW_POWER_MODE                  LP_MODE_STOP2   /* low-power mode to use between rounds during periods of inactivity */
#define LPM_DISABLE_GPIO_CLOCKS         0               /* set to 1 to disable GPIO clocks in low-power mode (-> no GPIO tracing possible) */
#define BASEBOARD_TREQ_WATCHDOG         3600            /* if != 0, the baseboard will be power-cycled if no time request has been received within the specified #seconds */

/* timesync */
#define TIMESTAMP_TYPICAL_DRIFT         40    /* typical drift +/- in ppm (if exceeded, a warning will be issued) */
#define TIMESTAMP_MAX_DRIFT             100   /* max. allowed drift in ppm (higher values will be capped) */

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

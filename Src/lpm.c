/*
 * lpm.c
 * Low-power mode management and application state machine
 *
 *  Created on: Apr 21, 2020
 *      Author: rdaforno
 */

/*
 * Notes:
 * - The radio is only put into sleep mode before entering an MCU deep sleep mode.
 * - The radio is NOT woken from sleep upon exit of a deep sleep mode (may not be in the interest of the application to do so).
 */

#include "main.h"


/* --- defines / macros --- */

#ifndef LPM_ON_IND
#define LPM_ON_IND()
#define LPM_OFF_IND()
#endif /* LPM_ON_IND */


/* --- globals --- */

extern LPTIM_HandleTypeDef hlptim1;
extern TIM_HandleTypeDef   htim1;
extern TIM_HandleTypeDef   htim2;
extern TIM_HandleTypeDef   htim16;
extern SPI_HandleTypeDef   hspi1;
extern SPI_HandleTypeDef   hspi2;
extern UART_HandleTypeDef  huart1;

extern void SystemClock_Config(void);        /* defined in main.c */


/* --- private variables --- */

/* operating mode and state machine */
static op_mode_t op_mode = OP_MODE_RESET;
static const op_mode_t op_mode_state_machine[NUM_OP_MODES][NUM_OP_MODE_EVENTS] =
{
  /* event:             OP_MODE_EVT_INIT, OP_MODE_EVT_WAKEUP, OP_MODE_EVT_DONE, OP_MODE_EVT_STOPPED, OP_MODE_EVT_RESTORED */
  /* state RESET: */  { OP_MODE_ACTIVE,   OP_MODE_RESET,      OP_MODE_RESET,    OP_MODE_RESET,       OP_MODE_RESET   },
  /* state ACTIVE: */ { OP_MODE_RESET,    OP_MODE_ACTIVE,     OP_MODE_IDLE,     OP_MODE_RESET,       OP_MODE_RESET   },
  /* state IDLE: */   { OP_MODE_RESET,    OP_MODE_ACTIVE,     OP_MODE_IDLE,     OP_MODE_LPM,         OP_MODE_RESET   },
  /* state LPM: */    { OP_MODE_RESET,    OP_MODE_WOKEN,      OP_MODE_RESET,    OP_MODE_RESET,       OP_MODE_RESET   },
  /* state WOKEN: */  { OP_MODE_RESET,    OP_MODE_WOKEN,      OP_MODE_RESET,    OP_MODE_RESET,       OP_MODE_ACTIVE  },
  /* NOTE: for all invalid transitions, go back to RESET state */
};
static lp_mode_t lp_mode = LOW_POWER_MODE;    /* selected low-power mode */


/* --- functions --- */

op_mode_t get_opmode(void)
{
  return op_mode;
}


void update_opmode(op_mode_event_t evt)
{
  op_mode = op_mode_state_machine[op_mode][evt];

  /* wakeup event? -> automatically resume from lpm */
  if (evt == OP_MODE_EVT_WAKEUP) {
    lpm_resume();
  }
}


/* prepare the MCU for low power mode */
void lpm_prepare(void)
{
#if configUSE_TICKLESS_IDLE
  /* stop the HAL tick independent of the operating mode if tickless idle is selected */
  HAL_SuspendTick();
#endif /* configUSE_TICKLESS_IDLE */

  /* only enter a low-power mode if the application is in idle state */
  if (op_mode == OP_MODE_IDLE)
  {
    if (lp_mode == LP_MODE_SLEEP) {
      /* do not update op_mode since we are already in IDLE and we are not entering a real LPM state */
      HAL_SuspendTick();
    }
    else if (lp_mode >= LP_MODE_STOP2) {

      /* make sure the radio is in sleep mode */
      radio_sleep(false);

      /* notes on stop mode:
      * - SRAM1, SRAM2 and all registers content are retained
      * - all clocks in the VCORE domain are stopped, the PLL, the MSI, the HSI16 and the HSE are disabled
      * - LSE and LPTIM keep running
      * - all I/O pins keep the state
      */
      ENTER_CRITICAL_SECTION();
      __DSB();
      __ISB();

      __HAL_RCC_PWR_CLK_ENABLE();

      SUSPEND_SYSTICK();
      HAL_SuspendTick();

      /* disable all unused peripherals */
      __HAL_TIM_DISABLE(&htim1);
      __HAL_TIM_DISABLE(&htim2);
      __HAL_TIM_DISABLE(&htim16);
      __HAL_UART_DISABLE(&huart1);
      __HAL_SPI_DISABLE(&hspi1);
      __HAL_SPI_DISABLE(&hspi2);
      /* for STANDBY and SHUTDOWN: also disable LPTIM1 */
      if (lp_mode == LP_MODE_STANDBY || lp_mode == LP_MODE_SHUTDOWN) {
        LPTIM_Disable(&hlptim1);    /* use this instead of __HAL_LPTIM_DISABLE(), implements an errata workaround */
      }

      /* configure HSI as clock source (16MHz) */
      RCC_OscInitTypeDef RCC_OscInitStruct = {0};
      RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
      RCC_OscInitStruct.OscillatorType        = RCC_OSCILLATORTYPE_HSI;
      RCC_OscInitStruct.HSIState              = RCC_HSI_ON;
      RCC_OscInitStruct.HSICalibrationValue   = RCC_HSICALIBRATION_DEFAULT;
      RCC_OscInitStruct.PLL.PLLState          = RCC_PLL_NONE;
      if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }
      /* select HSI as system clock */
      RCC_ClkInitStruct.ClockType     = RCC_CLOCKTYPE_SYSCLK;
      RCC_ClkInitStruct.SYSCLKSource  = RCC_SYSCLKSOURCE_HSI;
      if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) { Error_Handler(); }
      HAL_RCCEx_WakeUpStopCLKConfig(RCC_STOP_WAKEUPCLOCK_HSI);
      /* disable MSI */
      RCC_OscInitStruct.OscillatorType    = RCC_OSCILLATORTYPE_MSI;
      RCC_OscInitStruct.MSIState          = RCC_MSI_OFF;
      RCC_OscInitStruct.PLL.PLLState      = RCC_PLL_NONE;
      if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

      /* configure unused GPIOs for minimal current drain (make sure there are no floating inputs) */
      /* currently nothing to do */

      /* turn off LEDs */
      led_off(LED_EVENT);
      led_off(LED_SYSTEM);

      /* disable GPIO config clocks */
  #if LPM_DISABLE_GPIO_CLOCKS   /* if clocks are disabled, GPIO state cannot be changed in LPM */
      __HAL_RCC_GPIOA_CLK_DISABLE();
      __HAL_RCC_GPIOB_CLK_DISABLE();
      __HAL_RCC_GPIOC_CLK_DISABLE();
      __HAL_RCC_GPIOH_CLK_DISABLE();
  #endif /* LPM_DISABLE_GPIO_CLOCKS */

      /* disable and clear unused interrupts */
      HAL_NVIC_DisableIRQ(USART1_IRQn);
      HAL_NVIC_DisableIRQ(DMA1_Channel4_IRQn);
      HAL_NVIC_DisableIRQ(DMA1_Channel5_IRQn);
      HAL_NVIC_DisableIRQ(SPI1_IRQn);
      HAL_NVIC_DisableIRQ(SPI2_IRQn);
      HAL_NVIC_DisableIRQ(TIM1_UP_TIM16_IRQn);
      HAL_NVIC_DisableIRQ(TIM2_IRQn);
      /* note: do not disable LPTIM ARRM interrupt in LPM (see errata sheet) */

      /* configure RF_DIO1 on PC13 interrupt for wakeup from LPM */
      __HAL_GPIO_EXTI_CLEAR_IT(RADIO_DIO1_WAKEUP_Pin); // important for low-power consumption in STOP2 mode -> see README
      /*if (lp_mode == LP_MODE_STOP2) {
        HAL_NVIC_SetPriority(RADIO_DIO1_WAKEUP_EXTI_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(RADIO_DIO1_WAKEUP_EXTI_IRQn);
      }*/

      /* make sure the flags of all WAKEUP lines are cleared */
      __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

      if (lp_mode == LP_MODE_STOP2) {
        MODIFY_REG(PWR->CR1, PWR_CR1_LPMS, PWR_CR1_LPMS_STOP2);    /* set Stop mode 2 */
      }
      else if (lp_mode == LP_MODE_STANDBY) {
        MODIFY_REG(PWR->CR1, PWR_CR1_LPMS, PWR_CR1_LPMS_STANDBY);  /* set Standby mode */
      }
      else if (lp_mode == LP_MODE_SHUTDOWN) {
        MODIFY_REG(PWR->CR1, PWR_CR1_LPMS, PWR_CR1_LPMS_SHUTDOWN); /* set Shutdown mode */
      }
      SET_BIT(SCB->SCR, ((uint32_t)SCB_SCR_SLEEPDEEP_Msk));        /* set SLEEPDEEP bit */

      update_opmode(OP_MODE_EVT_STOPPED);
      LPM_ON_IND();

      LEAVE_CRITICAL_SECTION();      /* re-enable interrupts */
    }
  }
}


void lpm_resume(void)
{
  if (op_mode == OP_MODE_WOKEN) {
    /* MCU was in STOP2, STANDBY, or SHUTDOWN mode, different components need to be restored */

    /* make sure the following code runs atomically */
    ENTER_CRITICAL_SECTION();
    __DSB();
    __ISB();

    /* Reset SLEEPDEEP bit of Cortex System Control Register */
    CLEAR_BIT(SCB->SCR, ((uint32_t)SCB_SCR_SLEEPDEEP_Msk));

    /* make sure all required clocks are enabled */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    //__HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    //__HAL_RCC_FLASH_CLK_ENABLE();

    SystemClock_Config();                               /* restore clock config (and resume HAL tick) */

    /* restore GPIO config */
    /* currently nothing to do */

    /* restore peripherals */
    __HAL_TIM_ENABLE(&htim1);
    __HAL_TIM_ENABLE(&htim2);
    __HAL_TIM_ENABLE(&htim16);
    __HAL_UART_ENABLE(&huart1);
    __HAL_SPI_ENABLE(&hspi1);
    __HAL_SPI_ENABLE(&hspi2);

    /* re-enable interrupts */
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);
    HAL_NVIC_EnableIRQ(SPI2_IRQn);
    HAL_NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    /* disable RF_DIO1 on PC13 interrupt (only neede for wakeup from LPM) */
    HAL_NVIC_DisableIRQ(RADIO_DIO1_WAKEUP_EXTI_IRQn);
    __HAL_GPIO_EXTI_CLEAR_IT(RADIO_DIO1_WAKEUP_Pin);

    /* resume FreeRTOS SysTick */
    RESUME_SYSTICK();

    update_opmode(OP_MODE_EVT_RESTORED);
    LPM_OFF_IND();

    LEAVE_CRITICAL_SECTION();
  }

  /* make sure the HAL tick is resumed */
  HAL_ResumeTick();
}

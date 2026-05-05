/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"
#include "stm32f7xx_ll_rcc.h"
#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_system.h"
#include "stm32f7xx_ll_exti.h"
#include "stm32f7xx_ll_cortex.h"
#include "stm32f7xx_ll_utils.h"
#include "stm32f7xx_ll_pwr.h"
#include "stm32f7xx_ll_dma.h"
#include "stm32f7xx_ll_usart.h"
#include "stm32f7xx_ll_gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void MX_USB_OTG_HS_HCD_Init(void);
void MX_FMC_Init(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define PSRAM_NBL1_Pin LL_GPIO_PIN_1
#define PSRAM_NBL1_GPIO_Port GPIOE
#define PSRAM_NBL0_Pin LL_GPIO_PIN_0
#define PSRAM_NBL0_GPIO_Port GPIOE
#define PSRAM_NE1_Pin LL_GPIO_PIN_7
#define PSRAM_NE1_GPIO_Port GPIOD
#define NC1_Pin LL_GPIO_PIN_7
#define NC1_GPIO_Port GPIOB
#define LCD_PSRAM_D2_Pin LL_GPIO_PIN_0
#define LCD_PSRAM_D2_GPIO_Port GPIOD
#define LCD_NE_Pin LL_GPIO_PIN_9
#define LCD_NE_GPIO_Port GPIOG
#define LCD_PSRAM_NWE_Pin LL_GPIO_PIN_5
#define LCD_PSRAM_NWE_GPIO_Port GPIOD
#define LCD_PSRAM_D3_Pin LL_GPIO_PIN_1
#define LCD_PSRAM_D3_GPIO_Port GPIOD
#define LCD_PSRAM_NOE_Pin LL_GPIO_PIN_4
#define LCD_PSRAM_NOE_GPIO_Port GPIOD
#define PSRAM_A0_Pin LL_GPIO_PIN_0
#define PSRAM_A0_GPIO_Port GPIOF
#define PSRAM_A2_Pin LL_GPIO_PIN_2
#define PSRAM_A2_GPIO_Port GPIOF
#define PSRAM_A1_Pin LL_GPIO_PIN_1
#define PSRAM_A1_GPIO_Port GPIOF
#define PSRAM_A3_Pin LL_GPIO_PIN_3
#define PSRAM_A3_GPIO_Port GPIOF
#define PSRAM_A4_Pin LL_GPIO_PIN_4
#define PSRAM_A4_GPIO_Port GPIOF
#define PSRAM_A5_Pin LL_GPIO_PIN_5
#define PSRAM_A5_GPIO_Port GPIOF
#define PSRAM_A15_Pin LL_GPIO_PIN_5
#define PSRAM_A15_GPIO_Port GPIOG
#define PSRAM_A14_Pin LL_GPIO_PIN_4
#define PSRAM_A14_GPIO_Port GPIOG
#define PSRAM_A13_Pin LL_GPIO_PIN_3
#define PSRAM_A13_GPIO_Port GPIOG
#define LCD_PSRAM_D1_Pin LL_GPIO_PIN_15
#define LCD_PSRAM_D1_GPIO_Port GPIOD
#define PSRAM_A12_Pin LL_GPIO_PIN_2
#define PSRAM_A12_GPIO_Port GPIOG
#define PSRAM_A11_Pin LL_GPIO_PIN_1
#define PSRAM_A11_GPIO_Port GPIOG
#define LCD_PSRAM_D0_Pin LL_GPIO_PIN_14
#define LCD_PSRAM_D0_GPIO_Port GPIOD
#define PSRAM_A7_Pin LL_GPIO_PIN_13
#define PSRAM_A7_GPIO_Port GPIOF
#define PSRAM_A10_Pin LL_GPIO_PIN_0
#define PSRAM_A10_GPIO_Port GPIOG
#define LCD_PSRAM_D10_Pin LL_GPIO_PIN_13
#define LCD_PSRAM_D10_GPIO_Port GPIOE
#define PSRAM_A17_Pin LL_GPIO_PIN_12
#define PSRAM_A17_GPIO_Port GPIOD
#define PSRAM_A16_Pin LL_GPIO_PIN_11
#define PSRAM_A16_GPIO_Port GPIOD
#define LCD_PSRAM_D15_Pin LL_GPIO_PIN_10
#define LCD_PSRAM_D15_GPIO_Port GPIOD
#define PSRAM_A6_Pin LL_GPIO_PIN_12
#define PSRAM_A6_GPIO_Port GPIOF
#define PSRAM_A9_Pin LL_GPIO_PIN_15
#define PSRAM_A9_GPIO_Port GPIOF
#define LCD_PSRAM_D5_Pin LL_GPIO_PIN_8
#define LCD_PSRAM_D5_GPIO_Port GPIOE
#define LCD_PSRAM_D6_Pin LL_GPIO_PIN_9
#define LCD_PSRAM_D6_GPIO_Port GPIOE
#define LCD_PSRAM_D8_Pin LL_GPIO_PIN_11
#define LCD_PSRAM_D8_GPIO_Port GPIOE
#define LCD_PSRAM_D11_Pin LL_GPIO_PIN_14
#define LCD_PSRAM_D11_GPIO_Port GPIOE
#define LCD_PSRAM_D14_Pin LL_GPIO_PIN_9
#define LCD_PSRAM_D14_GPIO_Port GPIOD
#define LCD_PSRAM_D13_Pin LL_GPIO_PIN_8
#define LCD_PSRAM_D13_GPIO_Port GPIOD
#define PSRAM_A8_Pin LL_GPIO_PIN_14
#define PSRAM_A8_GPIO_Port GPIOF
#define LCD_PSRAM_D4_Pin LL_GPIO_PIN_7
#define LCD_PSRAM_D4_GPIO_Port GPIOE
#define LCD_PSRAM_D7_Pin LL_GPIO_PIN_10
#define LCD_PSRAM_D7_GPIO_Port GPIOE
#define LCD_PSRAM_D9_Pin LL_GPIO_PIN_12
#define LCD_PSRAM_D9_GPIO_Port GPIOE
#define LCD_PSRAM_D12_Pin LL_GPIO_PIN_15
#define LCD_PSRAM_D12_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

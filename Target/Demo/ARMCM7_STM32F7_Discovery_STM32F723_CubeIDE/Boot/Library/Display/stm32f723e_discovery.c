/**
  ******************************************************************************
  * @file    stm32f723e_discovery.c
  * @author  MCD Application Team
  * @brief   This file provides a set of firmware functions to manage LEDs,
  *          push-buttons, external PSRAM, external QSPI Flash, TS available on 
  *          STM32F723E-Discovery board (MB1260) from STMicroelectronics.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Dependencies
- stm32f7xx_hal_cortex.c
- stm32f7xx_hal_gpio.c
- stm32f7xx_hal_uart.c
- stm32f7xx_hal_i2c.c
- stm32f7xx_hal_sram.c
- stm32f7xx_hal_rcc_ex.c
EndDependencies */

/* Includes ------------------------------------------------------------------*/
#include "stm32f723e_discovery.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup STM32F723E_DISCOVERY
  * @{
  */

/** @defgroup STM32F723E_DISCOVERY_LOW_LEVEL STM32F723E-DISCOVERY LOW LEVEL
  * @{
  */

/** @defgroup STM32F723E_DISCOVERY_LOW_LEVEL_Private_TypesDefinitions STM32F723E Discovery Low Level Private Typedef
  * @{
  */
typedef struct
{
  __IO uint16_t REG;
  __IO uint16_t RAM;
}LCD_CONTROLLER_TypeDef;
/**
  * @}
  */

/** @defgroup STM32F723E_DISCOVERY_LOW_LEVEL_Private_Defines LOW_LEVEL Private Defines
  * @{
  */
/* We use BANK2 as we use FMC_NE2 signal */
#define FMC_BANK2_BASE  ((uint32_t)(0x60000000 | 0x04000000))  
#define FMC_BANK2       ((LCD_CONTROLLER_TypeDef *) FMC_BANK2_BASE)
/**
  * @}
  */


static void     FMC_BANK2_WriteData(uint16_t Data);
static void     FMC_BANK2_WriteReg(uint8_t Reg);
static uint16_t FMC_BANK2_ReadData(void);
static void     FMC_BANK2_Init(void);
static void     FMC_BANK2_MspInit(void);

/* LCD IO functions */
void            LCD_IO_Init(void);
void            LCD_IO_WriteData(uint16_t RegValue);
void            LCD_IO_WriteReg(uint8_t Reg);
void            LCD_IO_WriteMultipleData(uint16_t *pData, uint32_t Size);
uint16_t        LCD_IO_ReadData(void);
void            LCD_IO_Delay(uint32_t Delay);

/**
  * @}
  */

/** @defgroup STM32F723E_DISCOVERY_BSP_Public_Functions BSP Public Functions
  * @{
  */

/*******************************************************************************
                            LINK OPERATIONS
*******************************************************************************/
/*************************** FMC Routines *************************************/
/**
  * @brief  Initializes FMC_BANK2 MSP.
  * @retval None
  */
static void FMC_BANK2_MspInit(void)
{
  GPIO_InitTypeDef gpio_init_structure;
    
  /* Enable FMC clock */
  __HAL_RCC_FMC_CLK_ENABLE();
    
  /* Enable FSMC clock */
  __HAL_RCC_FMC_CLK_ENABLE();
  __HAL_RCC_FMC_FORCE_RESET();
  __HAL_RCC_FMC_RELEASE_RESET();
  
  /* Enable GPIOs clock */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE(); 
  
  /* Common GPIO configuration */
  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Pull      = GPIO_PULLUP;
  gpio_init_structure.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init_structure.Alternate = GPIO_AF12_FMC;
  
  /* GPIOD configuration */ 
  /* LCD_PSRAM_D2, LCD_PSRAM_D3, LCD_PSRAM_NOE, LCD_PSRAM_NWE, PSRAM_NE1, LCD_PSRAM_D13, 
     LCD_PSRAM_D14, LCD_PSRAM_D15, PSRAM_A16, PSRAM_A17, LCD_PSRAM_D0, LCD_PSRAM_D1 */
  gpio_init_structure.Pin   = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5  | GPIO_PIN_7 | GPIO_PIN_8 |\
                              GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOD, &gpio_init_structure);

  /* GPIOE configuration */
  /* PSRAM_NBL0, PSRAM_NBL1, LCD_PSRAM_D4, LCD_PSRAM_D5, LCD_PSRAM_D6, LCD_PSRAM_D7, 
     LCD_PSRAM_D8, LCD_PSRAM_D9, LCD_PSRAM_D10, LCD_PSRAM_D11, LCD_PSRAM_D12 */
  gpio_init_structure.Pin  = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 |GPIO_PIN_10 |\
                             GPIO_PIN_11 | GPIO_PIN_12 |GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOE, &gpio_init_structure);
  
  /* GPIOF configuration */
  /* PSRAM_A0, PSRAM_A1, PSRAM_A2, PSRAM_A3, PSRAM_A4, PSRAM_A5, 
     PSRAM_A6, PSRAM_A7, PSRAM_A8, PSRAM_A9 */
  gpio_init_structure.Pin  = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 |\
                             GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOF, &gpio_init_structure);

  /* GPIOG configuration */
  /* PSRAM_A10, PSRAM_A11, PSRAM_A12, PSRAM_A13, PSRAM_A14, PSRAM_A15, LCD_NE */
  gpio_init_structure.Pin  = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 |\
                             GPIO_PIN_9 ;  
  HAL_GPIO_Init(GPIOG, &gpio_init_structure);
}


/**
  * @brief  Initializes LCD IO.
  * @retval None
  */ 
static void FMC_BANK2_Init(void) 
{  
  SRAM_HandleTypeDef         hsram;
  FMC_NORSRAM_TimingTypeDef  sram_timing;

  /* GPIO configuration for FMC signals (LCD) */
  FMC_BANK2_MspInit();

  /* PSRAM device configuration */
  hsram.Instance  = FMC_NORSRAM_DEVICE;
  hsram.Extended  = FMC_NORSRAM_EXTENDED_DEVICE;

  /* Set parameters for LCD access (FMC_NORSRAM_BANK2) */
  hsram.Init.NSBank             = FMC_NORSRAM_BANK2;
  hsram.Init.DataAddressMux     = FMC_DATA_ADDRESS_MUX_DISABLE;
  hsram.Init.MemoryType         = FMC_MEMORY_TYPE_SRAM;
  hsram.Init.MemoryDataWidth    = FMC_NORSRAM_MEM_BUS_WIDTH_16;
  hsram.Init.BurstAccessMode    = FMC_BURST_ACCESS_MODE_DISABLE;
  hsram.Init.WaitSignalPolarity = FMC_WAIT_SIGNAL_POLARITY_LOW;
  hsram.Init.WaitSignalActive   = FMC_WAIT_TIMING_BEFORE_WS;
  hsram.Init.WriteOperation     = FMC_WRITE_OPERATION_ENABLE;
  hsram.Init.WaitSignal         = FMC_WAIT_SIGNAL_DISABLE;
  hsram.Init.ExtendedMode       = FMC_EXTENDED_MODE_DISABLE;
  hsram.Init.AsynchronousWait   = FMC_ASYNCHRONOUS_WAIT_DISABLE;
  hsram.Init.WriteBurst         = FMC_WRITE_BURST_DISABLE;
  hsram.Init.WriteFifo          = FMC_WRITE_FIFO_DISABLE;
  hsram.Init.PageSize           = FMC_PAGE_SIZE_NONE;
  hsram.Init.ContinuousClock    = FMC_CONTINUOUS_CLOCK_SYNC_ONLY;

  /* PSRAM device configuration */
  /* Timing configuration derived from system clock (up to 216Mhz)
     for 108Mhz as PSRAM clock frequency */
  sram_timing.AddressSetupTime      = 9;
  sram_timing.AddressHoldTime       = 2;
  sram_timing.DataSetupTime         = 6;
  sram_timing.BusTurnAroundDuration = 1;
  sram_timing.CLKDivision           = 2;
  sram_timing.DataLatency           = 2;
  sram_timing.AccessMode            = FMC_ACCESS_MODE_A;

  /* Initialize the FMC controller for LCD (FMC_NORSRAM_BANK2) */
  HAL_SRAM_Init(&hsram, &sram_timing, &sram_timing);
}

/**
  * @brief  Writes register value.
  * @param  Data: Data to be written 
  * @retval None
  */
static void FMC_BANK2_WriteData(uint16_t Data) 
{
  /* Write 16-bit Reg */
  FMC_BANK2->RAM = Data;
  __DSB();
}

/**
  * @brief  Writes register address.
  * @param  Reg: Register to be written
  * @retval None
  */
static void FMC_BANK2_WriteReg(uint8_t Reg) 
{
  /* Write 16-bit Index, then write register */
  FMC_BANK2->REG = Reg;
  __DSB();
}

/**
  * @brief  Reads register value.
  * @retval Read value
  */
static uint16_t FMC_BANK2_ReadData(void) 
{
  return FMC_BANK2->RAM;
}

/*******************************************************************************
                            LINK OPERATIONS
*******************************************************************************/

/********************************* LINK LCD ***********************************/

/**
  * @brief  Initializes LCD low level.
  * @retval None
  */
void LCD_IO_Init(void) 
{
  FMC_BANK2_Init();
}

/**
  * @brief  Writes data on LCD data register.
  * @param  RegValue: Register value to be written
  * @retval None
  */
void LCD_IO_WriteData(uint16_t RegValue) 
{
  /* Write 16-bit Reg */
  FMC_BANK2_WriteData(RegValue);
  __DSB();
}

/**
  * @brief  Writes several data on LCD data register.
  * @param  pData: pointer on data to be written
  * @param  Size: data amount in 16bits short unit
  * @retval None
  */
void LCD_IO_WriteMultipleData(uint16_t *pData, uint32_t Size)
{
  uint32_t  i;

  for (i = 0; i < Size; i++)
  {
    FMC_BANK2_WriteData(pData[i]);
    __DSB();
  }
}

/**
  * @brief  Writes register on LCD register.
  * @param  Reg: Register to be written
  * @retval None
  */
void LCD_IO_WriteReg(uint8_t Reg) 
{
  /* Write 16-bit Index, then Write Reg */
  FMC_BANK2_WriteReg(Reg);
  __DSB();
}

/**
  * @brief  Reads data from LCD data register.
  * @retval Read data.
  */
uint16_t LCD_IO_ReadData(void) 
{
  return FMC_BANK2_ReadData();
}

/**
  * @brief  LCD delay
  * @param  Delay: Delay in ms
  * @retval None
  */
void LCD_IO_Delay(uint32_t Delay)
{
  HAL_Delay(Delay);
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */


/**
  ******************************************************************************
  * @file    stm32f723e_discovery.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for STM32F723E-Discovery LEDs,
  *          push-buttons hardware resources.
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
/* IMPORTANT: One of the following flags must be defined in the preprocessor */
/* options in order to select the target board revision: !!!!!!!!!! */
/* USE_STM32F723E_DISCO */          /* Applicable for all boards execept STM32F723E DISCOVERY REVD */
/* USE_STM32F723E_DISCO_REVD */     /* Applicable only for STM32F723E DISCOVERY REVD */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32F723E_DISCOVERY_H
#define __STM32F723E_DISCOVERY_H

#ifdef __cplusplus
 extern "C" {
#endif


 /* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup STM32F723E_DISCOVERY
  * @{
  */

/** @addtogroup STM32F723E_DISCOVERY_LOW_LEVEL
  * @{
  */

/** @defgroup STM32F723E_DISCOVERY_LOW_LEVEL_Exported_Types STM32F723E Discovery Low Level Exported Types
 * @{
 */

/** 
  * @brief  Define for STM32F723E_DISCOVERY board
  */ 
#if !defined(USE_STM32F723E_DISCO_REVD) && \
    !defined(USE_STM32F723E_DISCO)
#define USE_STM32F723E_DISCO
#endif


/** @brief DISCO_Status_TypeDef
  *  STM32F723E_DISCO board Status return possible values.
  */
typedef enum
{
 DISCO_OK    = 0,
 DISCO_ERROR = 1

} DISCO_Status_TypeDef;

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

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* __STM32F723E_DISCOVERY_H */


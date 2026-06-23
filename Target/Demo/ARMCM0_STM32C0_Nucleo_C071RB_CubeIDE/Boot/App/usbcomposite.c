/************************************************************************************//**
* \file         Demo/ARMCM0_STM32C0_Nucleo_C071RB_CubeIDE/Boot/App/usbcomposite.c
* \brief        USB composite device for OpenBLT source file.
* \ingroup      Boot_ARMCM0_STM32C0_Nucleo_C071RB_CubeIDE
* \internal
*----------------------------------------------------------------------------------------
*                          C O P Y R I G H T
*----------------------------------------------------------------------------------------
*   Copyright (c) 2026  by Feaser    http://www.feaser.com    All rights reserved
*
*----------------------------------------------------------------------------------------
*                            L I C E N S E
*----------------------------------------------------------------------------------------
* This file is part of OpenBLT. OpenBLT is free software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License as published by the Free
* Software Foundation, either version 3 of the License, or (at your option) any later
* version.
*
* OpenBLT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
* PURPOSE. See the GNU General Public License for more details.
*
* You have received a copy of the GNU General Public License along with OpenBLT. It
* should be located in ".\Doc\license.html". If not, contact Feaser to obtain a copy.
*
* \endinternal
****************************************************************************************/

/****************************************************************************************
* Include files
****************************************************************************************/
#include "boot.h"                                /* bootloader generic header          */
#include "tusb.h"                                /* TinyUSB stack                      */
#include "stm32c0xx.h"                           /* STM32 CPU and HAL header           */


/****************************************************************************************
* Configuration check
****************************************************************************************/
/* The TinyUSB port was modified to support polling mode operation, needed by the
 * bootloader. Verify that polling mode was actually enabled in the configuration header.
 */
#if (CFG_TUSB_POLLING_ENABLED <= 0)
#error "CFG_TUSB_POLLING_ENABLED must be > 0"
#endif


/****************************************************************************************
* External function prototypes
****************************************************************************************/
#if (CFG_TUD_MSC > 0)
extern void UsbMscTask(void);
#endif


/************************************************************************************//**
** \brief     Initializes the USB composite device. Should be called once during software
**            program initialization.
** \return    none.
**
****************************************************************************************/
void UsbCompositeInit(void)
{
  PCD_HandleTypeDef usbHandle;

  /* Make sure the USB power and clock are enabled and that the GPIO pins are configured.
   * Note that this is only actually used for CubeMX generated projects. These projects
   * should be configured to not call MX_USB_PCD_Init() to do this. This is because
   * MX_USB_PCD_Init() relies on a running SysTick, which is not the case at the point
   * where the CubeMX generate code calls MX_USB_PCD_Init(). For non CubeMX generated
   * projects, the call to HAL_PCD_MspInit() simply goes to the empty "__weak" function.
   */
  usbHandle.Instance = USB_DRD_FS;
  HAL_PCD_MspInit(&usbHandle);
  /* Initialize the TinyUSB device stack on the configured roothub port */
  tud_init(BOARD_TUD_RHPORT);
} /*** end of UsbCompositeInit ***/


/************************************************************************************//**
** \brief     Releases the USB composite device.
** \return    none.
**
****************************************************************************************/
void UsbCompositeFree(void)
{
  PCD_HandleTypeDef usbHandle;

  /* Disconnect the TinyUSB device stack.*/
  tud_disconnect();
  /* Disable the USB power and clock and undo the the GPIO pins are configuration. Note
   * that this is only actually used for CubeMX generated projects. For non CubeMX
   * generated projects, the call to HAL_PCD_MspDeInit() simply goes to the empty
   * "__weak" function.
   */
  usbHandle.Instance = USB_DRD_FS;
  HAL_PCD_MspDeInit(&usbHandle);
} /*** end of UsbCompositeFree ***/


/************************************************************************************//**
** \brief     Task function of the USB composite device. Should be called continuously in
**            the program loop.
** \return    none.
**
****************************************************************************************/
void UsbCompositeTask(void)
{
  /* Poll for USB interrupt flags to process USB related events and run the USB device
   * stack task.
   */
  tud_int_handler(BOARD_TUD_RHPORT);
  tud_task();
#if (CFG_TUD_MSC > 0)
  /* Run the USB mass storage class device task. */
  UsbMscTask();
#endif
} /*** end of UsbCompositeTask ***/


/*********************************** end of usbcomposite.c *****************************/

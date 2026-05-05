/************************************************************************************//**
* \file         display.c
* \brief        Display module for firmware updates source file.
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
#include <string.h>                              /* for string utilities               */
#include "boot.h"                                /* bootloader generic header          */
#include "display.h"                             /* display module header              */
#include "stm32f723e_discovery_lcd.h"            /* Board LCD header                   */


/****************************************************************************************
* Macro definitions
****************************************************************************************/
/** \brief Maximum number of text characters that can be displayed, excluding nul
 *         termination.
 */
#define DISPLAY_MAX_TEXT_LEN           (20U)


/****************************************************************************************
* Local data declarations
****************************************************************************************/
/** \brief Data object for grouping together all display related data. */
static struct
{
  blt_int8u progress;                       /**< Firmware update progress percentage.  */
  blt_char  text[DISPLAY_MAX_TEXT_LEN+1];   /**< Info text. +1 for nul character.      */
} displayData;


/****************************************************************************************
* Function prototypes
****************************************************************************************/
static void DisplayOn(void);
static void DisplayOff(void);
static void DisplayClear(void);


/************************************************************************************//**
** \brief     Initializes the display module. Should be called once during
**            software program initialization.
** \return    none.
**
****************************************************************************************/
void DisplayInit(void)
{
  /* Initialize locals. */
  displayData.progress = 0U;
  displayData.text[0] = '\0';
  /* Perform low-level init. */
  BSP_LCD_Init();
  /* Turn off the display to prevent weird effect when its cleared. */
  DisplayOff();
  /* Clear the display. */
  DisplayClear();
  /* Turn on the display. */
  DisplayOn();
} /*** end of DisplayInit ***/


/************************************************************************************//**
** \brief     Uninitializes the display module. Should be called once during software
**            program exit.
** \return    none.
**
****************************************************************************************/
void DisplayDeInit(void)
{
  SRAM_HandleTypeDef hsram;

  /* Turn off the display. */
  DisplayOff();
  /* Clear the display. */
  DisplayClear();
  /* Perform low-level de-init. */
  BSP_LCD_DeInit();
  hsram.Instance    = FMC_NORSRAM_DEVICE;
  hsram.Extended    = FMC_NORSRAM_EXTENDED_DEVICE;
  hsram.Init.NSBank = FMC_NORSRAM_BANK2;
  HAL_SRAM_DeInit(&hsram);
} /*** end of DisplayDeInit ***/


/************************************************************************************//**
** \brief     Updates the firmware update progress on the display.
** \param     progress Firmware update progress as a percentage (0..100).
** \return    none.
**
****************************************************************************************/
void DisplaySetProgress(blt_int8u progress)
{
  /* Sanitize parameter. */
  progress = (progress > 100U) ? 100U : progress;

  /* Update the progress. */
  if (displayData.progress != progress)
  {
    /* Store the new progress. */
    displayData.progress = progress;
    /* Draw progress bar outline. */
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_DrawRect(19U, 19U, 201U, 32U);
    /* Draw progress bar content. */
    BSP_LCD_SetTextColor(LCD_COLOR_LIGHTGREEN);
    BSP_LCD_FillRect(20U, 20U, displayData.progress * 2U, 30U);
  }
} /*** end of DisplaySetProgress ***/


/************************************************************************************//**
** \brief     Updates the info text on the display.
** \param     text Info text to display with up to DISPLAY_MAX_TEXT_LEN characters.
** \return    none.
**
****************************************************************************************/
void DisplaySetText(blt_char * text)
{
  blt_int8u strLen;
  blt_int8u idx;
  blt_bool  changed = BLT_FALSE;

  /* Extract string length. */
  strLen = strlen(text);
  /* Sanitize parameter. */
  strLen = (strLen > DISPLAY_MAX_TEXT_LEN) ? DISPLAY_MAX_TEXT_LEN : strLen;
  /* Did the text change? */
  for (idx = 0U; idx < strLen; idx++)
  {
    if (text[idx] != displayData.text[idx])
    {
      changed = BLT_TRUE;
      break;
    }
  }
  /* Only update if the text changed. */
  if (changed == BLT_TRUE)
  {
    /* Store the new text. */
    for (idx = 0U; idx < strLen; idx++)
    {
      displayData.text[idx] = text[idx];
    }
    /* Add string termination. */
    displayData.text[strLen] = '\0';
    /* Clear the old text.*/
    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
    BSP_LCD_FillRect(0U, 60U, 240U, 16U);
    /* Draw the next text. */
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_DisplayStringAt(19U, 60U, (uint8_t *)displayData.text, LEFT_MODE);
  }
} /*** end of DisplaySetText ***/


/************************************************************************************//**
** \brief     Turns the display on.
** \return    none.
**
****************************************************************************************/
static void DisplayOn(void)
{
  /* Turn on the display. */
  BSP_LCD_DisplayOn();
  /* Turn on the backlight. */
  HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_PORT, LCD_BL_CTRL_PIN, GPIO_PIN_SET);
} /*** end of DisplayOn ***/


/************************************************************************************//**
** \brief     Turns the display off.
** \return    none.
**
****************************************************************************************/
static void DisplayOff(void)
{
  /* Turn off the backlight. */
  HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_PORT, LCD_BL_CTRL_PIN, GPIO_PIN_RESET);
  /* Turn off the display. */
  BSP_LCD_DisplayOff();
} /*** end of DisplayOff ***/


/************************************************************************************//**
** \brief     Clears the contents of the display.
** \return    none.
**
****************************************************************************************/
static void DisplayClear(void)
{
  /* Clear contents. */
  BSP_LCD_Clear(LCD_COLOR_BLACK);
} /*** end of DisplayClear ***/


/*********************************** end of display.c **********************************/

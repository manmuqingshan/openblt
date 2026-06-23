/************************************************************************************//**
* \file         Demo/ARMCM0_STM32C0_Nucleo_C071RB_CubeIDE/Boot/App/usbcdc.c
* \brief        USB CDC custom communication interface for OpenBLT source file.
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


/****************************************************************************************
* Background information
****************************************************************************************/
/* This source file implements a custom XCP communication interface for the OpenBLT
 * bootloader. It builds on the TinyUSB communication stack to construct a CDC-ACM
 * device.
 *
 * This custom XCP communication interface makes it possible to select RS232 as the
 * communcation interface in the MicroBoot (or BootCommander) and perform updates via
 * USB as a virtual COM-port.
 *
 * Before starting the firmware update, first make sure that the bootloader is running.
 * To run the bootloader, keep the blue pushbutton on the board pressed, while you reset
 * the board. Your PC will recognize a new serial port:
 *
 * - On Linux it is typically /dev/ttyACM0.
 * - On Windows you'll see a new entry in the Device Manager's section Ports (COM & LPT).
 *
 * It was purely added for demonstration purposes, allowing you to have a reference for
 * how to build your own custom XCP communication interface for the OpenBLT bootloader.
 *
 * Thanks to this feature to add a custom XCP communication interface, you can perform
 * firmware updates via any type of communication interface, as long as you can somehow
 * embed an XCP packet in its communication packets. For example RS485 packets with a
 * custom layout, or SPI, I2C, etc.
 *
 * To keep this example implementation simple, it skips the support of the checksum byte
 * that XCP on RS232 supports. You also don't need it, because USB packets already
 * incorporate a checksum. Refer to macro BOOT_COM_RS232_CS_TYPE in the OpenBLT
 * source code for more info regarding the checksum byte.
 *
 * To embed the XCP packet in USB CDC-ACM packets, an XCP packet length byte was added
 * at the start of the packet:
 *    ---------------------------------------------------------------------------
 *   | Len of XCP packet |       XCP packet data[0..(Len of XCP packet-1)]       |
 *    ---------------------------------------------------------------------------
 *
 * This is needed in this case, because in MicroBoot (or BootCommander) we plan to
 * configure firmware updates via RS232. Therefore, we need to use the same layout used
 * for XCP packets on RS232.
 *
 * Because of this extra length byte at the start of a USB CDC-ACM packet, the
 * configuration macros BOOT_COM_CUSTOM_TX_MAX_DATA and BOOT_COM_CUSTOM_RX_MAX_DATA were
 * both set to 63. 63 bytes for the XCP packet plus the extra 1 for the length, makes 64
 * bytes in total, which is the size of the USB endpoint.
 */


/****************************************************************************************
* Configuration check
****************************************************************************************/
/* This module implements a custom XCP communication interface fo the OpenBLT bootloader.
 * Make sure it was enabled in the configuration header-file.
 */
#if (BOOT_COM_CUSTOM_ENABLE <= 0)
#error "BOOT_COM_CUSTOM_ENABLE must be > 0"
#endif


/****************************************************************************************
* Function prototypes
****************************************************************************************/
static blt_bool ComCustomReceiveByte(blt_int8u *data);


/************************************************************************************//**
** \brief     Initializes the custom communication interface.
** \return    none.
**
****************************************************************************************/
void ComCustomInitHook(void)
{
  /* Nothing to do here. */
} /*** end of ComCustomInitHook ***/


/************************************************************************************//**
** \brief     Releases the custom communication interface.
** \return    none.
**
****************************************************************************************/
void ComCustomFreeHook(void)
{
  /* Nothing to do here. */
} /*** end of ComCustomFreeHook ***/


/************************************************************************************//**
** \brief     Transmits a packet formatted for the communication interface.
** \param     data Pointer to byte array with data that it to be transmitted.
** \param     len  Number of bytes that are to be transmitted.
** \return    none.
**
****************************************************************************************/
void ComCustomTransmitPacketHook(blt_int8u *data, blt_int8u len)
{
  /* Verify validity of the len-paramenter. */
  ASSERT_RT(len <= BOOT_COM_CUSTOM_TX_MAX_DATA);

  /* First transmit the length of the packet. */
  tud_cdc_n_write(0, &len, 1);
  /* Next transmit the actual packet bytes. */
  tud_cdc_n_write(0, data, len);
  /* Make sure the transmission starts, even if the endpoint buffer is not yet full. */
  tud_cdc_n_write_flush(0);
} /*** end of ComCustomTransmitPacketHook ***/


/************************************************************************************//**
** \brief     Receives a communication interface packet if one is present.
** \param     data Pointer to byte array where the data is to be stored.
** \param     len Pointer where the length of the packet is to be stored.
** \return    BLT_TRUE if a packet was received, BLT_FALSE otherwise.
**
****************************************************************************************/
blt_bool ComCustomReceivePacketHook(blt_int8u *data, blt_int8u *len)
{
  blt_bool result = BLT_FALSE;
  static blt_int8u xcpCtoReqPacket[BOOT_COM_CUSTOM_RX_MAX_DATA+1];  /* one extra for length */
  static blt_int8u xcpCtoRxLength;
  static blt_bool  xcpCtoRxInProgress = BLT_FALSE;

  /* Start of cto packet received? */
  if (xcpCtoRxInProgress == BLT_FALSE)
  {
    /* Store the message length when received. */
    if (ComCustomReceiveByte(&xcpCtoReqPacket[0]) == BLT_TRUE)
    {
      if ( (xcpCtoReqPacket[0] > 0) &&
           (xcpCtoReqPacket[0] <= BOOT_COM_CUSTOM_RX_MAX_DATA) )
      {
        /* Indicate that a cto packet is being received. */
        xcpCtoRxInProgress = BLT_TRUE;
        /* reset packet data count */
        xcpCtoRxLength = 0;
      }
    }
  }
  else
  {
    /* Store the next packet byte. */
    if (ComCustomReceiveByte(&xcpCtoReqPacket[xcpCtoRxLength+1]) == BLT_TRUE)
    {
      /* Increment the packet data count. */
      xcpCtoRxLength++;

      /* Check to see if the entire packet was received. */
      if (xcpCtoRxLength == xcpCtoReqPacket[0])
      {
        /* Copy the packet data. */
        CpuMemCopy((blt_int32u)data, (blt_int32u)&xcpCtoReqPacket[1], xcpCtoRxLength);
        /* Done with cto packet reception. */
        xcpCtoRxInProgress = BLT_FALSE;
        /* Set the packet length. */
        *len = xcpCtoRxLength;
        /* Packet reception complete. Update the result. */
        result = BLT_TRUE;
      }
    }
  }

  /* Give the result back to the caller. */
  return result;
} /*** end of ComCustomReceivePacketHook ***/


/************************************************************************************//**
** \brief     Receives a communication interface byte if one is present.
** \param     data Pointer to byte where the data is to be stored.
** \return    BLT_TRUE if a byte was received, BLT_FALSE otherwise.
**
****************************************************************************************/
static blt_bool ComCustomReceiveByte(blt_int8u *data)
{
  blt_bool result = BLT_FALSE;
  blt_int32u count;

  /* USB received data available? */
  if (tud_cdc_n_available(0))
  {
    /* Read the next byte from the internal USB reception buffer. */
    count = tud_cdc_n_read(0, data, 1);
    /* Check read result. */
    if (count == 1)
    {
      result = BLT_TRUE;
    }
  }
  /* Give the result back to the caller. */
  return result;
} /*** end of ComCustomReceiveByte ***/


/*********************************** end of usbcdc.c ***********************************/

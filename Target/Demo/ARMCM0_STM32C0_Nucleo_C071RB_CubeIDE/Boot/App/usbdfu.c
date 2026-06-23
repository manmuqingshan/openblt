/************************************************************************************//**
* \file         Demo/ARMCM0_STM32C0_Nucleo_C071RB_CubeIDE/Boot/App/usbdfu.c
* \brief        USB device firmware upgrade for OpenBLT source file.
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
/* This USB device firmware upgrade example implementation allows you to perform a
 * firmware update with dfu-util (https://dfu-util.sourceforge.net/).
 *
 * To start the firmware update, first make sure that the bootloader is running. To run
 * the bootloader, keep the blue pushbutton on the board pressed, while you reset the
 * board.
 *
 * To check if the DFU device is recognized, you can run command:
 *
 * - dfu-util -l
 *
 * It should output something like:
 *
 * `Found DFU: [16c0:05dc] ver=0100, devnum=6, cfg=1, intf=0, alt=0, name="OpenBLT DFU"`
 *
 * To start a firmware update, you can run command:
 *
 * - dfu-util.exe -d 16C0 -a 0 -R -D <your-firmware-file>.bin
 *
 * By adding the "-R" command-line parameter, the newly programmed firmware is
 * automatically started after the successful completion of the firmware update.
 *
 * Make sure to use dfu-util version 0.11 or newer. Earlier versions might not be able
 * to properly hande a DFU device that is part of a USB composite device.
 */


/****************************************************************************************
* Function prototypes
****************************************************************************************/
static blt_int16u UsbDfuGetMaxBlocks(void);


/************************************************************************************//**
** \brief     Calculates the total number of flash memory blocks available to the user
**            program.
** \details   From a DFU perspective the flash memory to download/upload is split up into
**            an equal amount of blocks of transfer size CFG_TUD_DFU_XFER_BUFSIZE. Just
**            keep in mind that the first part of flash is reserved for the OpenBLT
**            bootloader and this reserved range should be excluded from DFU download/
**            upload transfers.
** \return    The total number of CFG_TUD_DFU_XFER_BUFSIZE size blocks available to the
**            user program.
**
****************************************************************************************/
static blt_int16u UsbDfuGetMaxBlocks(void)
{
  blt_int16u result;
  blt_int32u userProgFlashLen;
  blt_int32u bootloaderFlashLen;

  /* To determine the size of flash reserved for the bootloader, we need to subtract
   * the flash base address from the user program base address. This can be done with the
   * help of configuration macro BOOT_NVM_SIZE_KB and some smart bit-wise operations.
   *
   * Assume the user program start address is 0x08006000. The length of flash reserved
   * for the bootloader would then be 0x6000. We need to reset all the other bits in
   * front of it, yet in a way that it also works if more space is reserved for the
   * bootloader in the future. Creative use of BOOT_NVM_SIZE_KB allows this.
   *
   * Assume the microcontroller has 128kb (BOOT_NVM_SIZE_KB) flash in total. That means
   * its start and end addresses are:
   *   0x08000000 = %0000 1000 0000 0000 0000 0000 0000 0000
   *   0x0801FFFF = %0000 1000 0000 0001 1111 1111 1111 1111
   *
   * This means that you can determine the flash base address (which is also the
   * bootloader base address) by resetting these bits in 0x08006000:
   *                %0000 0000 0000 0001 1111 1111 1111 1111
   *
   * You can get that bit-mask with: (BOOT_NVM_SIZE_KB * 1024U) - 1U
   *
   * To get the flash reserved for the bootloader, you can invert this bitmask:
   * ~((BOOT_NVM_SIZE_KB * 1024U) - 1U):
   *                %1111 1111 1111 1110 0000 0000 0000 0000
   *
   * And use this mask to reset the bits of the user program start address:
   *   0x08006000   = %0000 1000 0000 0000 0110 0000 0000 0000
   *   reset mask   = %1111 1111 1111 1110 0000 0000 0000 0000
   *                  ----------------------------------------
   *                  %0000 0000 0000 0000 0110 0000 0000 0000 = 0x6000
   */
  bootloaderFlashLen = NvmGetUserProgBaseAddress() & ~(~((BOOT_NVM_SIZE_KB*1024U) - 1U));
  /* Now we can calculate the flash size available to the user program. */
  userProgFlashLen = (BOOT_NVM_SIZE_KB * 1024U) - bootloaderFlashLen;
  /* With this info we can calculate the total number of CFG_TUD_DFU_XFER_BUFSIZE size
   * blocks available to the user program.
   */
  result = userProgFlashLen / CFG_TUD_DFU_XFER_BUFSIZE;
  /* Give the result back to the caller. */
  return result;
} /*** end of UsbDfuGetMaxBlocks ***/


/************************************************************************************//**
** \brief     TinyUSB stack callback invoked right before tud_dfu_download_cb() or
**            tud_dfu_manifest_cb() to obtain the timeout in milliseconds for the next
**            download/manifest operation. During this period, USB host won't try to
**            communicate with us.
** \param     alt Used as partition numner in order to support multiple partitions like
**            FLASH, EEPROM, etc.
** \param     state DFU_DNBUSY for a download operation, DFU_MANIFEST for a manifest
**            operation.
** \return    Timeout in milliseconds for the next download/manifest operation.
**
****************************************************************************************/
uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state)
{
  blt_int32u result = 0U;

  /* Looking at the bootloader's flashLayout[] table used by the bootloader, the largest
   * sector is 2kb. The maximum erase time for 2kb is 40ms according to the datasheet.
   *
   * For this port, the bootloader does write operations in chunks of 512 bytes.
   * (FLASH_WRITE_BLOCK_SIZE). The maximum programming time for 64 bits (8 bytes) is
   * 125us according to the datasheet.
   * For 512 bytes this then is 125 * (512/8) = 8000us = 8ms.
   *
   * This means that the worst case scenario for the DFU_DNBUSY state results in a
   * time of 40 + 8 = 48. Round up to 50ms to allow for run-time overhead such as
   * watchdog handling.
   *
   * For the DFU_MANIFEST state, the erase operation will already be done. Therefore
   * the worst case timeout will be writing two chunks of 512 bytes. One for still
   * buffered data for the last block and one for the bootblock. This means the
   * worst case time is 2 * 8 = 16 ms. Round up to 25ms to allow for run-time overhead
   * such as watchdog handling and signature checksum calculation.
   */

  /* The current implementation supports a FLASH partition at alt=0. */
  if (alt == 0U)
  {
    if (state == DFU_DNBUSY)
    {
      result = 50U;
    }
    else if (state == DFU_MANIFEST)
    {
      result = 25U;
    }
  }
  /* Give the result back to the caller. */
  return result;
} /*** end of tud_dfu_get_timeout_cb ***/


/************************************************************************************//**
** \brief     TinyUSB stack callback invoked after the reception of DFU_DNLOAD with
**            length > 0, followed by DFU_GETSTATUS with state DFU_DNBUSY. This is where
**            the NVM programming should take place. Once finished, a call to
**            tud_dfu_finish_flashing() is required.
** \details   Note that the download operated happens in sequential order, one block
**            after the other. Even though a flash sector is larger then a DFU transfer
**            block, it is okay to erase the flash sector once the first block in this
**            sector is about to be programmed.
** \param     alt Used as partition numner in order to support multiple partitions like
**            FLASH, EEPROM, etc.
** \param     block_num DFU transfers are one block at a time with the size of a block
**            being CFG_TUD_DFU_XFER_BUFSIZE. This parameter is a zero-based index to
**            the block to operate on.
** \param     data Pointer to the byte array with data to program into NVM.
** \param     length Number of bytes in the data array.
** \return    none.
**
****************************************************************************************/
void tud_dfu_download_cb(uint8_t alt, uint16_t block_num, const uint8_t *data,
                         uint16_t length)
{
  blt_int8u                  status = DFU_STATUS_ERR_ADDRESS;
  blt_addr                   blockBase;
  blt_int8u volatile const * bytePtr;
  blt_int32u                 byteIdx;
  blt_addr                   eraseBaseAddr;
  blt_int32u                 eraseLen;

  /* The current implementation supports a FLASH partition at alt=0. */
  if (alt == 0U)
  {
    /* Only continue if the number of bytes to upload is in the user program flash
     * region.
     */
    if (block_num < UsbDfuGetMaxBlocks())
    {
      /* Only continue if the length does not exceed the block size. */
      if (length <= CFG_TUD_DFU_XFER_BUFSIZE)
      {
        /* Update the result for okay for now and only flag an error once detected. */
        status = DFU_STATUS_OK;
        /* No need to do anything of the length is zero. */
        if (length > 0U)
        {
          /* Calculate the base address in flash of this block. */
          blockBase = NvmGetUserProgBaseAddress() + (block_num*CFG_TUD_DFU_XFER_BUFSIZE);
          /* Is this the base address the same as the start of user program flash? This
           * signals the start of the firmware update.
           */
          if (blockBase == NvmGetUserProgBaseAddress())
          {
            /* Reinit the NVM driver because a new firmware update is about to start. */
            NvmInit();
          }
          /* Do a flash erased check. Start by initializing the pointer to the first
           * byte.
           */
          bytePtr = (blt_int8u volatile const *)blockBase;
          /* Loop through the flash area that is about to be written, one byte at a time. */
          for (byteIdx = 0U; byteIdx < length; byteIdx++)
          {
            /* Is this byte in flash memory not in the erased state? */
            if (bytePtr[byteIdx] != 0xFFU)
            {
              /* Erase the flash memory from this point on. */
              eraseBaseAddr = (blt_addr)(&bytePtr[byteIdx]);
              eraseLen = length - byteIdx;
              if (NvmErase(eraseBaseAddr, eraseLen) == BLT_FALSE)
              {
                /* Could not erase memory. Flag the error. */
                status = DFU_STATUS_ERR_ERASE;
              }
              /* No need to continue with the flash erased check. */
              break;
            }
          }
          /* Only continue with programming if no error was detected so far. */
          if (status == DFU_STATUS_OK)
          {
            if (blockBase == 0x08008200U)
            {
              CopService();
            }
            /* Program the block. */
            if (NvmWrite(blockBase, length, (blt_int8u *)data) != BLT_TRUE)
            {
              /* Could not program memory. Flag the error. */
              status = DFU_STATUS_ERR_WRITE;
            }
          }
        }
      }
    }
  }
  /* Inform the stack taht the flashing operation for download completed. */
  tud_dfu_finish_flashing(status);
} /*** end of tud_dfu_download_cb ***/


/************************************************************************************//**
** \brief     TinyUSB stack callback invoked after the reception of DFU_DNLOAD with
**            length = 0, followed by DFU_GETSTATUS with state DFU_MANIFEST. This happens
**            at the end of the download. This is where a checksum could be calculated
**            and written and any buffered data can be programmed to NVM. Once finished,
**            a call to tud_dfu_finish_flashing() is required.
** \param     alt Used as partition numner in order to support multiple partitions like
**            FLASH, EEPROM, etc.
** \return    none.
**
****************************************************************************************/
void tud_dfu_manifest_cb(uint8_t alt)
{
  blt_int8u status = DFU_STATUS_OK;

  /* Calculate and write the signature checkum and write potentially still buffered
   * firmware data to flash.
   */
  if (NvmDone() != BLT_TRUE)
  {
    /* Update the status to flag the error. */
    status = DFU_STATUS_ERR_WRITE;
  }
  /* Inform the stack that the flashing operation completed. */
  tud_dfu_finish_flashing(status);
} /*** end of tud_dfu_manifest_cb ***/


/************************************************************************************//**
** \brief     TinyUSB stack callback invoked after the reception of DFU_UPLOAD.
** \param     alt Used as partition numner in order to support multiple partitions like
**            FLASH, EEPROM, etc.
** \param     block_num DFU transfers are one block at a time with the size of a block
**            being CFG_TUD_DFU_XFER_BUFSIZE. This parameter is a zero-based index to
**            the block to operate on.
** \param     length Number of bytes to write to the data array.
** \return    The number of bytes that were written to the data array.
**
****************************************************************************************/
uint16_t tud_dfu_upload_cb(uint8_t alt, uint16_t block_num, uint8_t *data,
                           uint16_t length)
{
  blt_int16u result = 0U;
  blt_addr   blockBase;
  blt_int16u blockDataLen = CFG_TUD_DFU_XFER_BUFSIZE;

  /* The current implementation supports a FLASH partition at alt=0. */
  if (alt == 0U)
  {
    /* Only continue if the number of bytes to upload is in the user program flash
     * region.
     */
    if (block_num < UsbDfuGetMaxBlocks())
    {
      /* Calculate the base address in flash of this block. */
      blockBase = NvmGetUserProgBaseAddress() + (block_num * CFG_TUD_DFU_XFER_BUFSIZE);
      /* Max sure the specified length is not larger than one block. */
      if (length < CFG_TUD_DFU_XFER_BUFSIZE)
      {
        blockDataLen = length;
      }
      /* Copy data from flash to the data buffer. */
      CpuMemCopy((blt_addr)data, blockBase, blockDataLen);
      /* Update the result. */
      result = blockDataLen;
    }
  }
  /* Give the result back to the caller. */
  return result;
} /*** end of tud_dfu_upload_cb ***/


/************************************************************************************//**
** \brief     TinyUSB stack callback invoked when the USB host terminated a download or
**            upload transfer.
** \param     alt Used as partition numner in order to support multiple partitions like
**            FLASH, EEPROM, etc.
** \return    none.
**
****************************************************************************************/
void tud_dfu_abort_cb(uint8_t alt)
{
  /* Nothing further needs to be done here at this point. */
} /*** end of tud_dfu_abort_cb ***/


/************************************************************************************//**
** \brief     TinyUSB stack callback invoked when a DFU_DETACH request is received. This
**            is where the user program should be started, if present.
** \return    none.
**
****************************************************************************************/
void tud_dfu_detach_cb(void)
{
  /* Attempt to start the user program. */
  CpuStartUserProgram();
} /*** end of tud_dfu_detach_cb ***/


/*********************************** end of usbdfu.c ***********************************/

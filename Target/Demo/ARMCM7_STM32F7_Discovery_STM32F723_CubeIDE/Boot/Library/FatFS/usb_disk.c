/************************************************************************************//**
* \file         usb_disk.c
* \brief        USB disk port for FatFS based on TinyUSB MSC host source file.
* \internal
*----------------------------------------------------------------------------------------
*                          C O P Y R I G H T
*----------------------------------------------------------------------------------------
*   Copyright (c) 2026  by Feaser    http://www.feaser.com    All rights reserved
*
*----------------------------------------------------------------------------------------
*                            L I C E N S E
*----------------------------------------------------------------------------------------
*
* SPDX-License-Identifier: MIT
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* \endinternal
****************************************************************************************/

/****************************************************************************************
* Include files
****************************************************************************************/
#include "boot.h"                                /* OpenBLT bootloader header          */
#include "usb_disk.h"                            /* USB disk module header             */
#include "tusb.h"                                /* TinyUSB header                     */
#include "ff.h"                                  /* FatFS generic header               */
#include "diskio.h"                              /* FatFS diskio header                */


/****************************************************************************************
* Configuration macros
****************************************************************************************/
#ifndef BOOT_FILE_USB_HOST_BACKDOOR_EXTENSION_MS
/** \brief For firmware updates from a FAT file system on a USB MSC device, the timed
 *         backdoor needs to stay open at least long enough for the USB host to detect
 *         and enumerate the USB MSC device. This configuration macro controls how much
 *         longer the default timed backdoor stays open.
 */
#define BOOT_FILE_USB_HOST_BACKDOOR_EXTENSION_MS (2000u)
#endif

#ifndef BOOT_FILE_DISK_IO_WAIT_TIMEOUT_MS
/** \brief Maximum time to wait to disk I/O operations to complete. Might need to be
 *         longer for large capacity disks. */
#define BOOT_FILE_DISK_IO_WAIT_TIMEOUT_MS        (2000u)
#endif

#ifndef BOOT_FILE_START_WHEN_MOUNTED_ENABLE
/** \brief When enabled this module already checks if a firmware update should be started
 *         to moment a USB disk is mounted. When disabled, this check is still done, just
 *         at a later time, when the timed backdoor expires. Added benefit of this
 *         feature is that it also automatically starts the firmware update when the
 *         bootloader remains running when no user program was started. So after the
 *         timed backdoor expiration event.
 */
#define BOOT_FILE_START_WHEN_MOUNTED_ENABLE      (1)
#endif


/****************************************************************************************
* Local data declarations
****************************************************************************************/
/** \brief Boolean flag to track if the USB stick is inserted or not. */
static bool usbDiskPresent;

/** \brief Boolean flag to track if IO operations are ongoing. */
static bool usbDiskBusy;


/************************************************************************************//**
** \brief     Initializes the USB disk module. Should be called once during software
**            program initialization.
** \return    none.
**
****************************************************************************************/
void UsbDiskInit(void)
{
  tusb_rhport_init_t host_init;

  /* Initialize locals. */
  usbDiskPresent = false;
  usbDiskBusy = false;

#if (BOOT_BACKDOOR_HOOKS_ENABLE == 0) && (BOOT_FILE_SYS_ENABLE > 0)
  /* Extend the time that the backdoor is open in case the default timed backdoor
   * mechanism is used. Needs to be extended to allow for enough time for the USB host to
   * detect a USB MSC device.
   */
  if (BackDoorGetExtension() < BOOT_FILE_USB_HOST_BACKDOOR_EXTENSION_MS)
  {
    BackDoorSetExtension(BOOT_FILE_USB_HOST_BACKDOOR_EXTENSION_MS);
  }
#endif /* BOOT_BACKDOOR_HOOKS_ENABLE == 0 */

  /* Initialize the TinyUSB host stack on the configured roothub port. */
  host_init.role = TUSB_ROLE_HOST;
  host_init.speed = TUSB_SPEED_AUTO;
  tusb_init(BOARD_TUH_RHPORT, &host_init);
} /*** edn of UsbDiskInit ***/


/************************************************************************************//**
** \brief     Task function of the USB disk module. Should be called continuously
**            in the program loop.
** \return    none.
**
****************************************************************************************/
void UsbDiskTask(void)
{
  /* Service the watchdog. */
  CopService();
  /* Poll for USB interrupt flags to process USB releated event and run the USB host
   * stack task.
   */
  tusb_int_handler(BOARD_TUH_RHPORT, false);
  tuh_task();
} /*** end of UsbDiskTask ***/


/************************************************************************************//**
** \brief     Utility function to determine if a disk is currently inserted or not. Can
**            be used by the application to detect disk insertion and removal events.
** \return    none.
**
****************************************************************************************/
blt_bool UsbDiskIsPresent(void)
{
  return usbDiskPresent;
} /*** end of UsbDiskIsPresent ***/


/****************************************************************************************
*                            T I N Y U S B   F U N C T I O N S
****************************************************************************************/

/************************************************************************************//**
** \brief     Obtain the current value of the millisecond counter.
** \return    Milliscond counter value.
**
****************************************************************************************/
uint32_t tusb_time_millis_api(void)
{
  return TimerGet();
} /*** end of tusb_time_millis_api ***/


/************************************************************************************//**
** \brief     Callback that gets called when a USB disk is mounted (inserted).
** \param     dev_addr Device address.
**
****************************************************************************************/
void tuh_msc_mount_cb(uint8_t dev_addr)
{
  (void)dev_addr;

#if (BOOT_FILE_START_WHEN_MOUNTED_ENABLE > 0) && (BOOT_FILE_SYS_ENABLE > 0)
  /* Now that a disk is detected, check if a firmware update should be started.  */
  (void)FileHandleFirmwareUpdateRequest();
#endif /* BOOT_FILE_START_WHEN_MOUNTED_ENABLE > 0 */
  /* Update disk present flag. */
  usbDiskPresent = true;
} /*** end of tuh_msc_mount_cb ***/


/************************************************************************************//**
** \brief     Callback that gets called when a USB disk is unmounted (removed).
** \param     dev_addr Device address.
**
****************************************************************************************/
void tuh_msc_umount_cb(uint8_t dev_addr)
{
  (void)dev_addr;

  /* Update disk present flag. */
  usbDiskPresent = false;
} /*** end of tuh_msc_umount_cb ***/


/****************************************************************************************
*                       F A T F S   F U N C T I O N S
****************************************************************************************/

/************************************************************************************//**
** \brief     Utility function to wait for read and write operations to complete.
** \param     pdrv Physical drive number to identify the drive.
** \return    True if the I/O operation completed successfully, false if they did not
**            complete with the configured timeout time.
**
****************************************************************************************/
static bool disk_wait_for_io_completion(BYTE pdrv)
{
  bool     result = true;
  uint32_t startMillis;
  uint32_t currentMillis;
  uint32_t deltaMillis;

  /* Unused parameter. This module only supports one mounted drive. */
  (void)pdrv;

  /* Store start time. */
  startMillis = tusb_time_millis_api();

  /* Run the task function until the read or write operation completes. */
  while (usbDiskBusy == true)
  {
    /* Run the USB disk task to poll and process USB related events. */
    UsbDiskTask();

    /* Did the maximum wait time pass? */
    currentMillis = tusb_time_millis_api();
    deltaMillis = currentMillis - startMillis;
    if (deltaMillis >= BOOT_FILE_DISK_IO_WAIT_TIMEOUT_MS)
    {
      /* Update the result to timeout error and stop looping. */
      result = false;
      break;
    }
  }

  /* Give the result back to the caller. */
  return result;
} /*** end of disk_wait_for_io_completion ***/


/************************************************************************************//**
** \brief     Callback function that gets calls by the TinyUSB stack when a read or
**            write operation completes.
** \param     pdrv Physical drive number to identify the drive.
**
****************************************************************************************/
static bool disk_io_complete(uint8_t dev_addr, const tuh_msc_complete_data_t *cb_data)
{
  (void)dev_addr;
  (void)cb_data;

  /* Reset the busy flag. */
  usbDiskBusy = false;

  return true;
} /*** end of disk_io_complete ***/


/************************************************************************************//**
** \brief     Initialize disk drive.
** \param     pdrv Physical drive number to identify the drive.
** \return    Disk status.
**
****************************************************************************************/
DSTATUS disk_initialize(BYTE pdrv)
{
  (void)pdrv;

  /* Nothing needs to be done here. */
  return 0;
} /*** end of disk_initialize ***/


/************************************************************************************//**
** \brief     Get disk status
** \param     pdrv Physical drive number to identify the drive.
** \return    Disk status.
**
****************************************************************************************/
DSTATUS disk_status(BYTE pdrv)
{
  DSTATUS result = STA_NODISK;
  uint8_t dev_addr;

  /* Obtain the device address. */
  dev_addr = pdrv + 1;

  /* Check if the disk is mounted. */
  if (tuh_msc_mounted(dev_addr))
  {
    /* All is good with the disk status. */
    result = 0;
  }

  /* Give the result back to the caller. */
  return result;
} /*** end of disk_status ***/


/************************************************************************************//**
** \brief     Read sector(s).
** \param     pdrv Physical drive number to identify the drive.
** \param     buff Data buffer to store read data.
** \param     sector Sector address in LBA.
** \param     count Number of sectors to read.
** \return    Disk status.
**
****************************************************************************************/
DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
  DRESULT       result = RES_OK;
  uint8_t       dev_addr;
  const uint8_t lun = 0;

  /* Obtain the device address. */
  dev_addr = pdrv + 1;
  /* Set the busy flag. */
  usbDiskBusy = BLT_TRUE;
  /* Initiate the read operation. */
  tuh_msc_read10(dev_addr, lun, buff, sector, (uint16_t)count, disk_io_complete, 0);
  /* Poll for completion. */
  if (disk_wait_for_io_completion(pdrv) == false)
  {
    result = RES_ERROR;
  }
  /* Give the result back to the caller. */
  return result;
} /*** end of disk_read ***/


/************************************************************************************//**
** \brief     Write sector(s).
** \param     pdrv Physical drive number to identify the drive.
** \param     buff Data to be written.
** \param     sector Sector address in LBA.
** \param     count Number of sectors to write.
** \return    Disk status.
**
****************************************************************************************/
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
  DRESULT       result = RES_OK;
  uint8_t       dev_addr;
  const uint8_t lun = 0;

  /* Obtain the device address. */
  dev_addr = pdrv + 1;
  /* Set the busy flag. */
  usbDiskBusy = BLT_TRUE;
  /* Initiate the write operation. */
  tuh_msc_write10(dev_addr, lun, buff, sector, (uint16_t)count, disk_io_complete, 0);
  /* Poll for completion. */
  if (disk_wait_for_io_completion(pdrv) == false)
  {
    result = RES_ERROR;
  }
  /* Give the result back to the caller. */
  return result;
} /*** end of disk_write ***/


/************************************************************************************//**
** \brief     Miscellaneous functions.
** \param     pdrv Physical drive number to identify the drive.
** \param     cmd Control code.
** \param     buff Buffer to send/receive data block.
** \return    Disk result.
**
****************************************************************************************/
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
  DRESULT       result;
  uint8_t       dev_addr;
  const uint8_t lun = 0;

  /* Obtain the device address. */
  dev_addr = pdrv + 1;

  /* Filter on the command. */
  switch (cmd)
  {
    case CTRL_SYNC:
      /* Nothing to do since we do blocking. */
      result = RES_OK;
      break;

    case GET_SECTOR_COUNT:
      *((DWORD *)buff) = (DWORD)tuh_msc_get_block_count(dev_addr, lun);
      result = RES_OK;
      break;

    case GET_SECTOR_SIZE:
      *((WORD *)buff) = (WORD)tuh_msc_get_block_size(dev_addr, lun);
      result = RES_OK;
      break;

    case GET_BLOCK_SIZE:
      *((DWORD *)buff) = 1; // erase block size in units of sector size
      result = RES_OK;
      break;

    default:
      result = RES_PARERR;
      break;
  }

  /* Give the result back to the caller. */
  return result;
} /*** end of disk_ioctl ***/


/*********************************** end of usb_disk.c *********************************/

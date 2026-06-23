/************************************************************************************//**
* \file         Demo/ARMCM0_STM32C0_Nucleo_C071RB_CubeIDE/Boot/App/usbmsc.c
* \brief        USB mass storage class for OpenBLT source file.
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
/* This USB mass storage class device example implementation allows you to perform a
 * firmware update, simply by dragging-and-dropping your firmware's binary file (*.bin)
 * to the drive.
 *
 * If the drive is not yet visible in your file manager, keep the blue pushbutton on the
 * board pressed, while you reset the board. This forces the bootloader to run and
 * shortly thereafter, a drive with label "OPENBLT" shows up in your file manager. It
 * contains one README.TXT file. At that point you can copy your firmware's binary
 * file (*.bin) to the drive to start the firmware update. After a successful firmware
 * update, the newly programmed firmware is automatically started.
 *
 * This USB mass storage class module, implements a FAT12 partition with a block/sector
 * size of 512 bytes. The total number of blocks is set to 261.
 *
 * The first 5 blocks are fixed and contain the bootsector, FAT1 and FAT2 allocation
 * tables, the root directory, and data cluster 2 with a README.TXT file. The remaining
 * 256 blocks represent 128kb, which is the flash memory available on this specific
 * microcontroller:
 *
 *   ---------------------------------------------------------------------------------
 *  |                          FAT12 partition                                        |
 *  |---------------------------------------------------------------------------------|
 *  |                                                                                 |
 *  |                   LBA 0 = Boot sector                                           |
 *  |                   LBA 1 = FAT1 allocation table                                 |
 *  |                   LBA 2 = FAT2 allocation table                                 |
 *  |                   LBA 3 = Root directory                                        |
 *  |                   LBA 4 = Data cluster 2 with README.TXT                        |
 *  |                                                                                 |
 *  |---------------------------------------------------------------------------------|
 *  |                                                                                 |
 *  |                   Remaining data clusters mapped to flash memory                |
 *  |                                                                                 |
 *   ---------------------------------------------------------------------------------
 *
 * The first 5 fixed blocks can be read, yet writes are ignored. When a firmware file
 * in binary format (".bin" extension) is copied to the storage device, it is written
 * to the part of flash that is available to the user program. Reads to this part of
 * the partition allways return zero values.
 */


/****************************************************************************************
* Macro definitions
****************************************************************************************/
/** \brief Size of a block on the storage device. */
#define USB_MSC_BLOCK_SIZE          (512U)

/** \brief The number of fixed blocks on the storage device. These are the ones that
 *         start with the boot sector and end with the README.TXT file contents.
 */
#define USB_MSC_FIXED_BLOCKS        (sizeof(usbMscFat12Partition)/USB_MSC_BLOCK_SIZE)

/** \brief The total number of blocks on the storage device. This is the fixed ones and
 *         the additional ones that could potentially cover the entire flash of the
 *         microcontroller. This value is stored at indices 19-20 in the boot sector.
 *         Note that not all this flash is actualy available to the user program though,
 *         as the first part is reserved for the bootloader.
 */
#define USB_MSC_TOTAL_BLOCKS        (((blt_int16u)(usbMscFat12Partition[0][20]) << 8U)| \
                                     usbMscFat12Partition[0][19])

/** \brief FAT archive attribute. When a file is copied to or modified on a FAT
 *         partition, it always gets the archive attribute set. Can be used to
 *         differentiate between other files and directories, such as the "System
 *         Volume Information" directory and a "IndexerVolumeGuid" file that Windows
 *         always created when inserting a USB MSC device for the first time.
 */
#define USB_MSC_FAT_ATTR_ARCHIVE    (0x20U)

/** \brief The root directory of a FAT12 partition at LBA 3 holds up to 16 entries of
 *         each 32 bytes.
 */
#define USB_MSC_FAT_ROOTDIR_ENTRIES (16U)

/** \brief The root directory logical block address of the FAT12 partition. It's the
 *         fourth block, so LBA 3.
 */
#define USB_MSC_FAT_ROOTDIR_LBA     (3U)


/****************************************************************************************
* Configuration check
****************************************************************************************/
#if (CFG_TUD_MSC_EP_BUFSIZE != USB_MSC_BLOCK_SIZE)
#error "CFG_TUD_MSC_EP_BUFSIZE should equal the FAT12 partition block size."
#endif


/****************************************************************************************
* Type definitions
****************************************************************************************/
/** \brief   Enumerated types with the mode in which files are copied to the FAT12
 *           partition.
 *  \details There are two modes:
 *           1. USB_MSC_MODE_ROOTDIR_FIRST: The host first updates the root directory
 *              (LBA 3) and then writes the file's data to the data clusters of the
 *              partition. Typical for Windows.
 *           2. USB_MSC_MODE_ROOTDIR_LAST: The host first writes the file's data to the
 *              data clusters of the partition and afterwards it updates the root
 *              directory (LBA 3). Typical for Linux.
 *           When no files have yet been copied, the mode is not yet know and therefore
 *           USB_MSC_MODE_UNKNOWN was added.
 */
typedef enum
{
  USB_MSC_MODE_UNKNOWN = 0,
  USB_MSC_MODE_ROOTDIR_FIRST,
  USB_MSC_MODE_ROOTDIR_LAST
} tUsbMscMode;

/** \brief Type for grouping together data for tracking firmware update related info. */
typedef struct
{
  tUsbMscMode mode;               /**< File copy mode used by the host.                */
  blt_bool    updateStarted;      /**< Flag to track if firmware update started.       */
  blt_int32u  fileSize;           /**< File size extracted from LBA 3 write command.   */
  blt_int32u  fileDataWritten;    /**< Number of bytes written to flash                */
  blt_int32u  startLba;           /**< Start LBA number for writing file data.         */
  blt_bool    finish;             /**< Request flag to finish the firmware update.    */
} tUsbMscUpdateInfo;

/** \brief Layout of a file or directory entry in the root directory of a FAT partition.
 *         LBA 3 is the root directory with up to 16 entries of each 32 bytes. This
 *         type matches the 32 bytes of such an entry.
 */
typedef struct TU_ATTR_PACKED
{
  blt_char   filename[8];                        /* Offset 0x00                        */
  blt_char   extension[3];                       /* Offset 0x08                        */
  blt_int8u  attributes;                         /* Offset 0x0B                        */
  blt_int8u  reserved_nt;                        /* Offset 0x0C                        */
  blt_int8u  create_tenths;                      /* Offset 0x0D                        */
  blt_int16u create_time;                        /* Offset 0x0E  (packed fat_time)     */
  blt_int16u create_date;                        /* Offset 0x10  (packed fat_date)     */
  blt_int16u access_date;                        /* Offset 0x12                        */
  blt_int16u cluster_high;                       /* Offset 0x14  (0 for FAT12/FAT16)   */
  blt_int16u write_time;                         /* Offset 0x16                        */
  blt_int16u write_date;                         /* Offset 0x18                        */
  blt_int16u cluster_low;                        /* Offset 0x1A                        */
  blt_int32u file_size;                          /* Offset 0x1C                        */
} tUsbMscRootDirEntry;


/****************************************************************************************
* Function prototypes
****************************************************************************************/
static blt_int16u UsbMscGetUserProgBlocks(void);
static blt_bool   UsbMscWriteDataToFlash(blt_int32u lba, blt_int32u offset,
                                         blt_int8u * data, blt_int32u len);
static blt_bool   UsbMscFinishFirmwareUpdate(void);


/****************************************************************************************
* Local data declarations
****************************************************************************************/
/** \brief Boolean flag to keep track if the MSC device was ejected from the USB host. */
static blt_bool usbMscEjected = BLT_FALSE;

/** \brief Data storage for tracking firmware update related info. */
static tUsbMscUpdateInfo usbMscUpdateInfo =
{
  USB_MSC_MODE_UNKNOWN,
  BLT_FALSE,
  0U,
  0U,
  0U,
  BLT_FALSE
};


/****************************************************************************************
* Local constant declarations
****************************************************************************************/
/** \brief   The FAT12 partition of the MSC device consists of 5 fixed blocks, followed
 *           by the flash memory that is available to the user program. This array
 *           contains the contents of these first 5 fixed blocks.
 *  \details Note that the total sectors is set to 261 at index 19-20 of the boot sector.
 *           That's these 5 fixed blocks and then 256 extra, which represents the 128kb
 *           flash of this particular microcontroller derivative. Keep in mind that if
 *           an array end with all zeroes, there is no need to actually add them in the
 *           array definition, since C99 automatically adds zeroes, if you omit them.
 */
static const uint8_t usbMscFat12Partition[][USB_MSC_BLOCK_SIZE] =
{
  {
    /* Boot sector - BPB with geometry (512 B/sector, 1 cluster, 261 total sectors),
     * OEM MSWIN4.1, volume label OPENBLT, filesystem type FAT12, and the 0x55 0xAA
     * signature at the end. The boot code area is all zeros (not executable). Note
     * the magic code in the last two bytes. These are mandatory.
     */
    0xEB, 0x3C, 0x90, 0x4D, 0x53, 0x57, 0x49, 0x4E, 0x34, 0x2E, 0x31, 0x00, 0x02, 0x01,
    0x01, 0x00, 0x02, 0x10, 0x00, 0x05, 0x01, 0xF0, 0x01, 0x00, 0x20, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x29, 0x78, 0x56, 0x34,
    0x12, 0x4F, 0x50, 0x45, 0x4E, 0x42, 0x4C, 0x54, 0x20, 0x20, 0x20, 0x20, 0x46, 0x41,
    0x54, 0x31, 0x32, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xAA
  },
  {
    /* FAT1 - File Allocation Table. Entry 0 = 0xFF0 (media descriptor 0xF0),
     * entry 1 = 0xFFF (reserved), entry 2 = 0xFFF (end-of-chain for README.TXT).
     * Entries 3+ are free (0x000).
     */
    0xF0, 0xFF, 0xFF, 0xFF, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  },
  {
    /* FAT2 - Exact mirror copy of FAT1. */
    0xF0, 0xFF, 0xFF, 0xFF, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  },
  {
    /* Root directory - 16 entries of 32 bytes each. Entry 0: volume label OPENBLT
     * (attr 0x08). Entry 1: file README  TXT (attr 0x00, cluster 2, size 160).
     * Entries 2–15: zeroed/free.
     */
    0x4F, 0x50, 0x45, 0x4E, 0x42, 0x4C, 0x54, 0x20, 0x20, 0x20, 0x20, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x52, 0x45, 0x41, 0x44, 0x4D, 0x45, 0x20, 0x20, 0x54, 0x58,
    0x54, 0x00, 0x00, 0x00, 0x00, 0x60, 0x21, 0x58, 0x21, 0x58, 0x00, 0x00, 0x00, 0x60,
    0x21, 0x58, 0x02, 0x00, 0xA0, 0x00, 0x00, 0x0,
  },
  {
    /* Data area, cluster 2 - Contains the contents of the README.TXT with zero padding
     * to the end of the sector.
     */
    0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x4F, 0x70, 0x65, 0x6E, 0x42, 0x4C,
    0x54, 0x27, 0x73, 0x20, 0x4D, 0x53, 0x43, 0x20, 0x64, 0x72, 0x69, 0x76, 0x65, 0x2E,
    0x20, 0x43, 0x6F, 0x70, 0x79, 0x20, 0x79, 0x6F, 0x75, 0x72, 0x20, 0x66, 0x69, 0x72,
    0x6D, 0x77, 0x61, 0x72, 0x65, 0x20, 0x74, 0x6F, 0x20, 0x74, 0x68, 0x69, 0x73, 0x20,
    0x64, 0x72, 0x69, 0x76, 0x65, 0x20, 0x74, 0x6F, 0x20, 0x73, 0x74, 0x61, 0x72, 0x74,
    0x20, 0x74, 0x68, 0x65, 0x20, 0x66, 0x69, 0x72, 0x6D, 0x77, 0x61, 0x72, 0x65, 0x0A,
    0x75, 0x70, 0x64, 0x61, 0x74, 0x65, 0x2E, 0x20, 0x4E, 0x6F, 0x74, 0x65, 0x20, 0x74,
    0x68, 0x61, 0x74, 0x20, 0x74, 0x68, 0x65, 0x20, 0x66, 0x69, 0x72, 0x6D, 0x77, 0x61,
    0x72, 0x65, 0x20, 0x66, 0x69, 0x6C, 0x65, 0x20, 0x73, 0x68, 0x6F, 0x75, 0x6C, 0x64,
    0x20, 0x62, 0x65, 0x20, 0x69, 0x6E, 0x20, 0x74, 0x68, 0x65, 0x20, 0x62, 0x69, 0x6E,
    0x61, 0x72, 0x79, 0x20, 0x66, 0x6F, 0x72, 0x6D, 0x61, 0x74, 0x20, 0x28, 0x2A, 0x2E,
    0x62, 0x69, 0x6E, 0x29, 0x2E, 0x0A
  },
};


/************************************************************************************//**
** \brief     Task function of the USB mass storage class device. Should be called
**            continuously in the program loop.
** \return    none.
**
****************************************************************************************/
void UsbMscTask(void)
{
  /* Request set to finish the firmware update? */
  if (usbMscUpdateInfo.finish == BLT_TRUE)
  {
    /* Reset all the update info except the mode. */
    usbMscUpdateInfo.updateStarted = BLT_FALSE;
    usbMscUpdateInfo.fileSize = 0U;
    usbMscUpdateInfo.fileDataWritten = 0U;
    usbMscUpdateInfo.startLba = 0U;
    usbMscUpdateInfo.finish = BLT_FALSE;
    /* Process firmware update complete event. */
    if (UsbMscFinishFirmwareUpdate() == BLT_FALSE)
    {
      /* No further action is needed here, since we just already reset all
       * the update info except the mode.
       */
    }
  }
} /*** end of UsbMscTask ***/


/************************************************************************************//**
** \brief     Calculates the total number of flash memory blocks on the storage device,
**            available to the user program.
** \details   The storage device is split up into a finite number of fixed blocks of
**            size USB_MSC_BLOCK_SIZE. So theoretically there are BOOT_NVM_SIZE_KB /
**            USB_MSC_BLOCK_SIZE blocks. However, the first part of flash is reserved
**            for the OpenBLT bootloader and this reserved range should be excluded from
** \return    The total number of USB_MSC_BLOCK_SIZE size blocks available to the
**            user program.
**
****************************************************************************************/
static blt_int16u UsbMscGetUserProgBlocks(void)
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
  result = userProgFlashLen / USB_MSC_BLOCK_SIZE;
  /* Give the result back to the caller. */
  return result;
} /*** end of UsbMscGetUserProgBlocks ***/


/************************************************************************************//**
** \brief     Writes data to the mapped flash memory.
** \param     lba Starting Logical Block Address to write to.
** \param     offset Byte offset within the starting block.
** \param     data Source data to write.
** \param     len Number of bytes to write (<= CFG_TUD_MSC_EP_BUFSIZE).
** \return    BLT_TRUE is successful, BLT_FALSE otherwise.
**
****************************************************************************************/
static blt_bool UsbMscWriteDataToFlash(blt_int32u lba, blt_int32u offset,
                                       blt_int8u * data, blt_int32u len)
{
  blt_bool                   result = BLT_FALSE;
  blt_addr                   writeBaseAddr = 0U;
  blt_int8u volatile const * bytePtr;
  blt_int32u                 byteIdx;
  blt_addr                   eraseBaseAddr;
  blt_int32u                 eraseLen;

  /* Write operations to flash are only allowed when a firmware update from a binary
   * file is actually in progress.
   */
  if (usbMscUpdateInfo.updateStarted == BLT_TRUE)
  {
    /* The LBA must be at least the same or higher than the starting LBA for the binary
     * file write operation.
     */
    if (lba >= usbMscUpdateInfo.startLba)
    {
      /* Make sure the lba is not outside the flash available to the user program,
       * relative to the starting LBA for the binary write operation.
       */
      if ((lba - usbMscUpdateInfo.startLba) < UsbMscGetUserProgBlocks())
      {
        /* The lba to write to is mapped within the flash that is available to the user
         * program. Update the result to reflect this.
         */
        result = BLT_TRUE;
      }
    }
  }

  /* Only continue of all is okay so far. At this point that means the data to write
   * will fit in the flash memory available to the user program.
   */
  if (result == BLT_TRUE)
  {
    /* Calculate the base address in flash memory for this write operation. */
    writeBaseAddr  = NvmGetUserProgBaseAddress();
    writeBaseAddr += (lba - usbMscUpdateInfo.startLba) * USB_MSC_BLOCK_SIZE;
    writeBaseAddr += offset;

    /* Do a flash erased check. Start by initializing the pointer to the first byte. */
    bytePtr = (blt_int8u volatile const *)writeBaseAddr;
    /* Loop through the flash area that is about to be written, one byte at a time. */
    for (byteIdx = 0U; byteIdx < len; byteIdx++)
    {
      /* Is this byte in flash memory not in the erased state? */
      if (bytePtr[byteIdx] != 0xFFU)
      {
        /* Erase the flash memory from this point on. */
        eraseBaseAddr = (blt_addr)(&bytePtr[byteIdx]);
        eraseLen = len - byteIdx;
        if (NvmErase(eraseBaseAddr, eraseLen) == BLT_FALSE)
        {
          /* Update the result for error. */
          result = BLT_FALSE;
        }
        /* No need to continue with the flash erased check. */
        break;
      }
    }
  }

  /* Only continue of all is okay so far. At this point that means the flash memory to
   * write is in the erased state.
   */
  if (result == BLT_TRUE)
  {
    /* Write the data to flash memory. */
    if (NvmWrite(writeBaseAddr, len, data) == BLT_FALSE)
    {
      /* Update the result for error. */
      result = BLT_FALSE;
    }
  }

  /* Give the result back to the caller. */
  return result;
} /*** end of UsbMscWriteDataToFlash ***/


/************************************************************************************//**
** \brief     Completes the firmware update and attempts to start the newly programmed
**            firmware. Note that this function does not return, if the newly programmed
**            firmware could be started.
** \return    BLT_TRUE is successful, BLT_FALSE otherwise.
**
****************************************************************************************/
static blt_bool UsbMscFinishFirmwareUpdate(void)
{
  blt_bool result = BLT_FALSE;

  /* Complete NVM write operation by committing still-to-be-written data to flash. */
  if (NvmDone() == BLT_TRUE)
  {
    /* Update the result for success. */
    result = BLT_TRUE;
    /* Attempt to start the user program. Note that if successful, this function call
     * does not return.
     */
    CpuStartUserProgram();
  }

  /* Give the result back to the caller. */
  return result;
} /*** end of UsbMscFinishFirmwareUpdate ***/


/************************************************************************************//**
** \brief     TinyUSB stack callback invoked when the host sends SCSI_CMD_INQUIRY v2.
** \param     lun Logical Unit Number being queried.
** \param     inquiry_resp Pointer to a pre-initialized scsi_inquiry_resp_t struct. The
**            stack zeroes it and sets defaults (is_removable=1, version=2,
**            response_data_format=2, additional_length=31). The application fills in
**            vendor_id[8], product_id[16], product_rev[4] and may override other fields.
** \param     bufsize Size of the response buffer (allows returning vendor-specific data
**            beyond the standard 36 bytes).
** \return    Length of the inquiry response written, typically
**            sizeof(scsi_inquiry_resp_t) (36 bytes). Can be longer if appending vendor-
**            specific data (up to bufsize). Returning 0 causes the stack to fall back to
**            the older v1 callback tud_msc_inquiry_cb()
**
****************************************************************************************/
uint32_t tud_msc_inquiry2_cb(uint8_t lun, scsi_inquiry_resp_t *inquiry_resp,
                             uint32_t bufsize)
{
  uint32_t result = sizeof(scsi_inquiry_resp_t);
  /* Set the vendor and product identifiers and the revision number. */
  const blt_char vid[] = "OpenBLT";
  const blt_char pid[] = "Mass Storage";
  const blt_char rev[] = "1.0";

  /* Unused parameters. */
  (void) lun;
  (void) bufsize;

  /* Store the vendor and product identifiers and the revision numberm in the inquiry
   * response struct.
   */
  CpuMemCopy((blt_addr)inquiry_resp->vendor_id,   (blt_addr)vid, sizeof(vid));
  CpuMemCopy((blt_addr)inquiry_resp->product_id,  (blt_addr)pid, sizeof(pid));
  CpuMemCopy((blt_addr)inquiry_resp->product_rev, (blt_addr)rev, sizeof(rev));
  /* Give the result back to the caller. */
  return result;
} /*** end of tud_msc_inquiry2_cb ***/


/************************************************************************************//**
** \brief     TinyUSB stack callback invoked when the host sends SCSI_CMD_TEST_UNIT_READY
**            to check if the logical unit is ready for data transfers.
** \param     lun Logical Unit Number being polled.
** \return    True if LUN is ready, false if not ready.
**
****************************************************************************************/
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  bool result = true;

  /* Unused parameter. */
  (void) lun;

  /* Was the storage device ejects from the host? */
  if (usbMscEjected == BLT_TRUE)
  {
    /* Set the sense to NOT_READY with the addtion of 3Ah 00h, which means NOT FOUND. */
    result = tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
  }
  /* Give the result back to the caller. */
  return result;
} /*** end of tud_msc_test_unit_ready_cb ***/


/************************************************************************************//**
** \brief     TinyUSB stack callback invoked when the host sends
**            SCSI_CMD_READ_CAPACITY_10 or SCSI_CMD_READ_FORMAT_CAPACITY to determine the
**            disk size.
** \param     lun Logical Unit Number.
** \param     block_count Write the total number of blocks here.
** \param     block_size Write the block size in bytes here (e.g., 512).
** \return    none.
**
****************************************************************************************/
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
  /* Unused parameter. */
  (void) lun;

  /* Set the block size to the size of one block of the FAT12 partition. */
  *block_size = USB_MSC_BLOCK_SIZE;
  /* Set the block count to the number of blocks. This is the first few blocks that
   * include the boot sector all the way up to the README.txt file. And then the blocks
   * covering the entire flash memory of the microcontroller. So the part available to
   * the bootloader and the user program.
   */
  *block_count  = USB_MSC_TOTAL_BLOCKS;
} /*** end of tud_msc_capacity_cb ***/


/************************************************************************************//**
** \brief     TinyUSB stack callback invoked when the host sends
**            SCSI_CMD_START_STOP_UNIT to power-control, start, stop, or eject/load the
**            medium.
** \details   - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
**            - Start = 1 : active mode, if load_eject = 1 : load disk storage
** \param     lun Logical Unit Number.
** \param     power_condition Power condition field from the SCSI command (e.g. 0=active,
**            1=idle, 2=standby, 3=low-power, 5=lu-control). Non-zero values request a
**            power-state transition.
** \param     start True to spin up / enter active mode; false to stop / enter low-power
**            mode.
** \param     load_eject True to eject (when start=false) or load (when start=true) the
**            medium; false for no mechanical action.
** \return    True if successful, false otherwise, for example when the medium is not
**            present.
**
****************************************************************************************/
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start,
                           bool load_eject)
{
  bool result = true;

  /* Unused parameters. */
  (void) lun;
  (void) power_condition;

  /* Request set to eject or load the disk storage? */
  if (load_eject)
  {
    /* Should the disk storage be loaded? */
    if (start)
    {
      usbMscEjected = BLT_FALSE;
    }
    /* Disk storate should be unloaded. */
    else
    {
      usbMscEjected = BLT_TRUE;
    }
  }
  /* Give the result back to the caller. */
  return result;
} /*** end of tud_msc_start_stop_cb ***/


/************************************************************************************//**
** \brief     TinyUSB stack callback invoked when the host sends SCSI_CMD_READ_10 to read
**            data from the storage medium.
** \param     lun Logical Unit Number.
** \param     lba Starting Logical Block Address to read from.
** \param     offset Byte offset within the starting block, used for partial-block reads.
** \param     buffer Destination buffer to fill with data.
** \param     bufsize Number of bytes requested by the host (<= CFG_TUD_MSC_EP_BUFSIZE).
** \return    TUD_MSC_RET_ERROR when the read operation was unsuccessful. Otherwise the
**            number of bytes actually read and placed in buffer. Must be <= bufsize. A
**            positive value less than bufsize signals "short read" (the stack completes
**            the transfer and does not retry).
**
****************************************************************************************/
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer,
                          uint32_t bufsize)
{
  int32_t           result = 0;
  blt_int32u        partitionTotalSize;
  blt_int8u const * readPtr;
  blt_int8u       * writePtr;
  blt_int32u        byteIdx;

  /* Unused parameter. */
  (void) lun;

  /* Calculate the total size of the partition. */
  partitionTotalSize = USB_MSC_TOTAL_BLOCKS * USB_MSC_BLOCK_SIZE;

  /* Is the block number outsize of the partition or is the requested data range outside
   * of the partition?
   */
  if ( (lba >= USB_MSC_TOTAL_BLOCKS) ||
       (((lba * USB_MSC_BLOCK_SIZE) + offset + bufsize) > partitionTotalSize) )
  {
    /* Out of range. Update the result accordingly. */
    result = TUD_MSC_RET_ERROR;
  }

  /* Only continue if no errors were detected. */
  if (result >= 0)
  {
    /* Initialize the read and write pointers. */
    readPtr  = (blt_int8u const *)(((blt_addr)&(usbMscFat12Partition[lba][0])) + offset);
    writePtr = (blt_int8u *)buffer;

    /* Read out the data one byte at a time but only allow reading from the fixed blocks
     * that contain the boot sector up to the README.TXT contents. Otherwise return
     * zeroes, because there is no need to read the flash contents as it won't actually
     * have FAT file system related entries on it.
     */
    for (byteIdx = 0U; byteIdx < bufsize; byteIdx++)
    {
      /* Still reading from the fixed blocks of the FAT12 partition? */
      if (readPtr < &usbMscFat12Partition[USB_MSC_FIXED_BLOCKS][0] )
      {
        /* Copy the byte to the buffer. */
        writePtr[byteIdx] = *readPtr;
      }
      /* Reading from the part that is mapped to user program flash. */
      else
      {
        /* No need to read out the actual firmware data from flash. Write a zero. */
        writePtr[byteIdx] = 0U;
      }
      /* Increment the read pointer for the next loop iteration. */
      readPtr++;

      /* Service the watchdog after a certain amount of loop iterations. */
      if ((byteIdx % 256U) == 0U)
      {
        CopService();
      }
    }
    /* Update the result for success. */
    result = bufsize;
  }

  /* Give the result back to the caller. */
  return result;
} /*** end of tud_msc_read10_cb ***/


/************************************************************************************//**
** \brief     TinyUSB stack callback invoked when the host sends SCSI_CMD_WRITE_10 or
**            SCSI_CMD_MODE_SENSE_6 to check if the medium is writable.
** \param     lun Logical Unit Number.
** \return    True if the medium is writable, false otherwise.
**
****************************************************************************************/
bool tud_msc_is_writable_cb(uint8_t lun)
{
  bool result;

  /* Unused parameter. */
  (void) lun;

  /* The storage device is writable. The to-be-written data will be stored in the flash
   * that is available to the user program. Note that writes to the first 5 blocks will
   * be ignored. The contains the boot sector of the FAT12 partition and the README.TXT.
   */
  result = true;
  /* Give the result back to the caller. */
  return result;
} /*** end of tud_msc_is_writable_cb ***/


/************************************************************************************//**
** \brief     TinyUSB stack callback invoked when the host sends SCSI_CMD_WRITE_10 to
**            write data to the medium.
** \param     lun Logical Unit Number.
** \param     lba Starting Logical Block Address to write to.
** \param     offset Byte offset within the starting block.
** \param     buffer Source data received from the host.
** \param     bufsize Number of bytes to write (<= CFG_TUD_MSC_EP_BUFSIZE).
** \return    If successful, the number of bytes actually written. If less than bufsize,
**            the remaining data is transferred first and the callback is invoked again.
**            If not successful:
**            - TUD_MSC_RET_ERROR: Write failed (e.g., invalid address).
**            - TUD_MSC_RET_BUSY: Disk I/O not ready.
**            - TUD_MSC_RET_ASYNC: I/O started asynchronously. The callback must return
**              immediately, and tud_msc_async_io_done() must be called once the
**              background I/O completes.
**
****************************************************************************************/
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer,
                           uint32_t bufsize)
{
  int32_t                     result = 0;
  blt_int32u                  partitionTotalSize;
  tUsbMscRootDirEntry const * rootDirEntry;
  blt_int8u                   entryIdx;

  /* Unused parameter. */
  (void) lun;

  /* Calculate the total size of the partition. */
  partitionTotalSize = USB_MSC_TOTAL_BLOCKS * USB_MSC_BLOCK_SIZE;

  /* Is the block number outsize of the partition or is the data range to write outside
   * of the partition?
   */
  if ( (lba >= USB_MSC_TOTAL_BLOCKS) ||
       (((lba * USB_MSC_BLOCK_SIZE) + offset + bufsize) > partitionTotalSize) )
  {
    /* Out of range. Update the result accordingly. */
    result = TUD_MSC_RET_ERROR;
  }

  /* Only continue if no errors were detected. */
  if (result >= 0)
  {
    /* Update the result for success for now and only change it upon error detection. */
    result = bufsize;

    /* Attempting to write the root directory? This will happen if a new file or
     * directory is created.
     */
    if (lba == USB_MSC_FAT_ROOTDIR_LBA)
    {
      /* Mode unknown? */
      if (usbMscUpdateInfo.mode == USB_MSC_MODE_UNKNOWN)
      {
        /* Was file data already written? This for example happens on Linux based
         * systems. The data first gets written to the sectors and afterwards the
         * root directory gets updated (LBA 3).
         */
        if (usbMscUpdateInfo.fileDataWritten > 0U)
        {
          /* Set the mode. */
          usbMscUpdateInfo.mode = USB_MSC_MODE_ROOTDIR_LAST;
        }
        /* No file data written yet. This for example happens on Windows based
         * systems. The root directory gets updated first (LBA 3) and then the data
         * gets written.
         */
        else
        {
          /* Set the mode. */
          usbMscUpdateInfo.mode = USB_MSC_MODE_ROOTDIR_FIRST;
        }
      }

      /* LBA 3 is the root directory with up to USB_MSC_FAT_ROOTDIR_ENTRIES entries of
       * each sizeof(tUsbMscRootDirEntry), being 32 bytes.
       */
      rootDirEntry = (tUsbMscRootDirEntry const *)buffer;
      for (entryIdx = 0U; entryIdx < USB_MSC_FAT_ROOTDIR_ENTRIES; entryIdx++)
      {
        /* Is this a normal file? Normal files copied to or modified on a FAT12 partition
         * get the FAT_ATTR_ARCHIVE (0x20) attribute set.
         */
        if (rootDirEntry[entryIdx].attributes == USB_MSC_FAT_ATTR_ARCHIVE)
        {
          /* It is a file with the .BIN extension? Note that filenames and their
           * extension always get written in the 8.3 short format in upper case.
           */
          if ( (rootDirEntry[entryIdx].extension[0] == 'B') &&
               (rootDirEntry[entryIdx].extension[1] == 'I') &&
               (rootDirEntry[entryIdx].extension[2] == 'N') )
          {
            /* Mode where the root directory gets written first, before the file's
             * data?
             */
            if (usbMscUpdateInfo.mode == USB_MSC_MODE_ROOTDIR_FIRST)
            {
              /* Firmware update not yet started? */
              if (usbMscUpdateInfo.updateStarted == BLT_FALSE)
              {
                /* Only process this as the start of a firmware update if the starting
                 * cluster of the file is plausible. The README.TXT is located at data
                 * cluster 2. This means new files need to be at a higher cluster.
                 * Additionally, the file size must be greater than zero.
                 */
                if ( (rootDirEntry[entryIdx].cluster_low > 2U) &&
                     (rootDirEntry[entryIdx].file_size > 0U) )
                {
                  /* Store the starting LBA for the file data that is about to come. Note
                   * that cluser_low is the starting cluster number of the file's data.
                   * The first free data cluster is 2 and that's where the README.TXT is
                   * located. If you look in usbMscFat12Partition[], you can see that
                   * data cluster 2 equals usbMscFat12Partition[4], so LBA = 4. Therefore
                   * to convert the cluster_low element stored in the entry to its
                   * corresponding LBA number for this particular FAT12 partition, you
                   * need to add the value 2.
                   */
                  usbMscUpdateInfo.startLba = rootDirEntry[entryIdx].cluster_low + 2U;
                  /* Store the file size. Needed to detect when the file data write
                   * operation is complete.
                   */
                  usbMscUpdateInfo.fileSize = rootDirEntry[entryIdx].file_size;
                  /* Reset the number of data written. */
                  usbMscUpdateInfo.fileDataWritten = 0U;
                  /* Set flag that the firmware update started. */
                  usbMscUpdateInfo.updateStarted = BLT_TRUE;
                  /* Reset finish request flag. */
                  usbMscUpdateInfo.finish = BLT_FALSE;
                  /* Reinit the NVM driver because a new firmware update is about to
                   * start.
                   */
                  NvmInit();
                }
              }
            }
            /* Mode where the root directory gets written last. */
            else if (usbMscUpdateInfo.mode == USB_MSC_MODE_ROOTDIR_LAST)
            {
              /* Firmware update in progress? */
              if (usbMscUpdateInfo.updateStarted == BLT_TRUE)
              {
                /* Set request to finish the firmware update to be processed at task
                 * level.
                 */
                usbMscUpdateInfo.finish = BLT_TRUE;
              }
            }
            /* File with the .BIN extension detected. No need to continue looping through
             * the other entries in the root directory.
             */
            break;
          }
        }
      }
    }
    /* Attempting to write the a data cluster after the one where the README.TXT is
     * located?
     */
    else if (lba >= USB_MSC_FIXED_BLOCKS)
    {
      /* Mode unknown? */
      if (usbMscUpdateInfo.mode == USB_MSC_MODE_UNKNOWN)
      {
        /* Was the file to which this data belongs already written to the root
         * directory? This for example happens on Windows based systems. The root
         * directory gets updated first (LBA 3) and then the data gets written.
         */
        if ( (usbMscUpdateInfo.fileSize > 0U) && (usbMscUpdateInfo.startLba > 0U) )
        {
          /* Set the mode. */
          usbMscUpdateInfo.mode = USB_MSC_MODE_ROOTDIR_FIRST;
        }
        /* The file to which this data belongs was not yet written to the root directory.
         * This for example happens on Linux based systems. The data first gets written
         * to the sectors and afterwards the root directory gets updated (LBA 3).
         */
        else
        {
          /* Set the mode. */
          usbMscUpdateInfo.mode = USB_MSC_MODE_ROOTDIR_LAST;
        }
      }

      /* Mode where the root directory gets written last. */
      if (usbMscUpdateInfo.mode == USB_MSC_MODE_ROOTDIR_LAST)
      {
        /* Firmware update not yet started? */
        if (usbMscUpdateInfo.updateStarted == BLT_FALSE)
        {
          /* Is this the first time data was written? */
          if (usbMscUpdateInfo.fileDataWritten == 0U)
          {
            /* Store the starting LBA for the file data. */
            usbMscUpdateInfo.startLba = lba;
            /* Reset the number of data written. */
            usbMscUpdateInfo.fileDataWritten = 0U;
            /* Set flag that the firmware update started. */
            usbMscUpdateInfo.updateStarted = BLT_TRUE;
            /* Reset finish request flag. */
            usbMscUpdateInfo.finish = BLT_FALSE;
            /* Reinit the NVM driver because a new firmware update is about to start. */
            NvmInit();
          }
        }
      }

      /* Firmware update still in progress? */
      if (usbMscUpdateInfo.updateStarted == BLT_TRUE)
      {
        /* Write the data to flash. */
        if (UsbMscWriteDataToFlash(lba, offset, buffer, bufsize) == BLT_TRUE)
        {
          /* Update the number of bytes of the firmware file that were successfully
           * written so far.
           */
          usbMscUpdateInfo.fileDataWritten += bufsize;
        }
        /* Error detected during the write operation. */
        else
        {
          /* Reset all the update info except the mode. */
          usbMscUpdateInfo.updateStarted = BLT_FALSE;
          usbMscUpdateInfo.fileSize = 0U;
          usbMscUpdateInfo.fileDataWritten = 0U;
          usbMscUpdateInfo.startLba = 0U;
          usbMscUpdateInfo.finish = BLT_FALSE;
        }
      }

      /* Mode where the root directory gets written first, before the file's
       * data?
       */
      if (usbMscUpdateInfo.mode == USB_MSC_MODE_ROOTDIR_FIRST)
      {
        /* Firmware update started? */
        if (usbMscUpdateInfo.updateStarted == BLT_TRUE)
        {
          /* All data written? */
          if (usbMscUpdateInfo.fileDataWritten >= usbMscUpdateInfo.fileSize)
          {
            /* Set request to finish the firmware update to be processed at task
             * level.
             */
            usbMscUpdateInfo.finish = BLT_TRUE;
          }
        }
      }
    }
  }

  /* Give the result back to the caller. */
  return result;
} /*** end of tud_msc_write10_cb ***/


/************************************************************************************//**
** \brief     TinyUSB stack callback invoked when the host sends a SCSI command that is
**            not handled by the built-in list: READ_CAPACITY_10, READ_FORMAT_CAPACITY,
**            INQUIRY, TEST_UNIT_READY, START_STOP_UNIT, MODE_SENSE6, REQUEST_SENSE,
**            READ_10, or WRITE_10.
** \param     lun Logical Unit Number.
** \param     scsi_cmd Raw 16-byte SCSI Command Descriptor Block (CDB) from the host.
** \param     buffer Data stage buffer. For INPUT commands the application fills this
**            with response data. For OUTPUT commands it contains data from the host.
** \param     bufsize Size of the buffer.
** \return    > 0: Number of bytes placed in buffer (input) or processed (output). The
**                 stack transmits this as the data-in stage.
**            0  : No-data command (command link OK). If the host expected data, the
**                 stack fails the command.
**            < 0: Error / unsupported command. The stack STALLs the endpoint and returns
**                 CHECK CONDITION.
**
****************************************************************************************/
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer,
                        uint16_t bufsize)
{
  int32_t result;

  /* Unused parameters. */
  (void) lun;
  (void) scsi_cmd;
  (void) buffer;
  (void) bufsize;

  /* At this time no additional commands to the built-in list are supported. Set the
   * result to a negative value to indicate the command request failed.
   */
  result = -1;
  /* Give the result back to the caller. */
  return result;
} /*** end of tud_msc_scsi_cb ***/


/*********************************** end of usbmsc.c ***********************************/

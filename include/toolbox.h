/** 
 * Copyright (C) 2023 Eric Helgeson
 * Copyright (C) 2024 Niels Martin Hansen
 * 
 * This file is based on the interface from BlueSCSI
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

#ifndef TOOLBOX_H
#define TOOLBOX_H

typedef struct {
    unsigned char index;   /* byte 00: file index in directory */
    unsigned char type;    /* byte 01: type 0 = file, 1 = directory */
    char name[33];         /* byte 02-34: filename (32 byte max) + space for NUL terminator */
    unsigned char size[5]; /* byte 35-39: file size (40 bit big endian unsigned) */

    unsigned long GetSize() const
    {
        return
            (unsigned long)size[1] << 24 |
            (unsigned long)size[2] << 16 |
            (unsigned long)size[3] <<  8 |
            (unsigned long)size[4];
    }
} ToolboxFileEntry;

typedef struct {
    char device_type[8];
} ToolboxDeviceList;

enum ToolboxDeviceType {
	TOOLBOX_DEVTYPE_FIXED = 0,
	TOOLBOX_DEVTYPE_REMOVEABLE,
	TOOLBOX_DEVTYPE_OPTICAL,
	TOOLBOX_DEVTYPE_FLOPPY_14MB,
	TOOLBOX_DEVTYPE_MO,
	TOOLBOX_DEVTYPE_SEQUENTIAL,
	TOOLBOX_DEVTYPE_NETWORK,
	TOOLBOX_DEVTYPE_ZIP100,
    
    TOOLBOX_DEVTYPE_NONE = 0xFF,
};


/** TOOLBOX_LIST_FILES (read, length 10)
 * Input:
 *  CDB 00 = command byte
 * Output:
 *  Array of ToolboxFileEntry structures. Max 100 structures.
 */
#define TOOLBOX_LIST_FILES     0xD0

/** TOOLBOX_GET_FILE (read, length 10)
 * Input:
 *  CDB 00 = command byte
 *  CDB 01 = file index byte
 *  CDB 02 = 32 bit block index to retrieve from the file
 *      03 | Blocks are 4096 bytes
 *      04 | Big endian
 *      05 | Must be 0 on first call for a file to open it
 * Output:
 *  Up to 4096 bytes of file data
 * Notes:
 *  The firmware expects the host to read files sequentially and completely.
 *  Reading a file is done when a read returns less than 4096 bytes.
 */
#define TOOLBOX_GET_FILE       0xD1

/** TOOLBOX_COUNT_FILES (read, length 10)
 * Input:
 *  CDB 00 = command byte
 * Output:
 *  Single byte indicating number of files in shared directory. (Max 100.)
 */
#define TOOLBOX_COUNT_FILES    0xD2

/** TOOLBOX_SEND_FILE_PREP (write, length 10)
 * Input:
 *  CDB 00 = command byte
 *  Data 00 = 32 byte filename to create
 *        . |
 *        . |
 *        . |
 *       31 |
 * Output:
 *  None.
 * Notes:
 *  Creates a new file in the shared directory and opens it for writing.
 *  Use TOOLBOX_SEND_FILE_10 to fill data into the file,
 *  and TOOLBOX_SEND_FILE_END to close it when done.
 */
#define TOOLBOX_SEND_FILE_PREP 0xD3

/** TOOLBOX_SEND_FILE_10 (write, length 10)
 * Input:
 *  CDB 00 = command byte
 *  CDB 01 = 16 bit data size (min 1, max 512)
 *      02 | Big endian
 *  CDB 03 = 24 bit block index to write to
 *      04 | Big endian
 *      05 | Blocks are 512 bytes
 *  Data   = up to 512 bytes of data (as per CDB 01-02)
 * Output:
 *  None.
 */
#define TOOLBOX_SEND_FILE_10   0xD4

/** TOOLBOX_SEND_FILE_END (write, length 10)
 * Input:
 *  CDB 00 = command byte
 * Output:
 *  None.
 */
#define TOOLBOX_SEND_FILE_END  0xD5

/** TOOLBOX_TOGGLE_DEBUG (read, length 10)
 * Input:
 *  CDB 00 = command byte
 *  CDB 01 = if byte 0, set debug state
 *         | if byte 1, get debug state
 *  CDB 02 = new debug state byte (only if CDB 01 = 0)
 * Output:
 *  If CDB 01 = 0, none.
 *  If CDB 01 = 1, single byte describing debug state.
 */
#define TOOLBOX_TOGGLE_DEBUG   0xD6

/** TOOLBOX_LIST_CDS (read, length 10)
 * Input:
 *  CDB 00 = command byte
 * Output:
 *  Array of ToolboxFileEntry structures. Max 100 structures.
 */
#define TOOLBOX_LIST_CDS       0xD7

/** TOOLBOX_SET_NEXT_CD (read, length 10)
 * Input:
 *  CDB 00 = command byte
 *  CDB 01 = image file index byte to change to
 * Output:
 *  None.
 */
#define TOOLBOX_SET_NEXT_CD    0xD8

/** TOOLBOX_LIST_DEVICES (read, length 10)
 * Input:
 *  CDB 00 = command byte
 * Output:
 *  8 bytes, each indicating the device type of the emulated SCSI devices
 *           or 0xFF for not-enabled targets
 */
#define TOOLBOX_LIST_DEVICES   0xD9

/** TOOLBOX_COUNT_CDS (read, length 10)
 * Input:
 *  CDB 00 = command byte
 * Output:
 *  Single byte indicating number of CD images available. (Max 100.)
 */
#define TOOLBOX_COUNT_CDS      0xDA

#define OPEN_RETRO_SCSI_TOO_MANY_FILES  0x0001

#endif /* TOOLBOX_H */

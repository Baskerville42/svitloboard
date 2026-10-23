/* Flash split for 512 KiB ESP8266 chips. */
/* sketch @0x40200000 (~463 KiB) (475120 B) */
/* empty  @0x40273FF0 (~4 KiB)  (4112 B) */
/* littlefs @0x40275000 (24 KiB) (24576 B) */
/* eeprom @0x4027B000 (4 KiB) */
/* rfcal  @0x4027C000 (4 KiB) */
/* wifi   @0x4027D000 (12 KiB) */

MEMORY
{
  dport0_0_seg :                        org = 0x3FF00000, len = 0x10
  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000
  irom0_0_seg :                         org = 0x40201010, len = 0x73ff0
}

PROVIDE ( _FS_start = 0x40275000 );
PROVIDE ( _FS_end = 0x4027B000 );
PROVIDE ( _FS_page = 0x100 );
PROVIDE ( _FS_block = 0x1000 );
PROVIDE ( _EEPROM_start = 0x4027B000 );
/* Deprecated compatibility symbols retained for the ESP8266 core. */
PROVIDE ( _SPIFFS_start = 0x40275000 );
PROVIDE ( _SPIFFS_end = 0x4027B000 );
PROVIDE ( _SPIFFS_page = 0x100 );
PROVIDE ( _SPIFFS_block = 0x1000 );

INCLUDE "local.eagle.app.v6.common.ld"

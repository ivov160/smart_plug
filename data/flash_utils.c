#include "flash_utils.h"



uint32_t crc32(uint32_t crc, const uint8_t *data, uint32_t size)
{
	static const uint32_t s_crc32[16] = {
		0,			0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c};

	if (data == NULL)
	{
		return 0;
  }

  uint32_t crcu32 = ~crc;
  while (size-- > 0) 
  { 
	  uint8_t b = *data++; 
	  crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b & 0xF)];
	  crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xF) ^ (b >> 4)]; 
  }
  return ~crcu32;
}



#include "crc32.h"

static uint32_t g_dwCRCTable[256];
static bool  g_bInitialized=false;

// Reflection is a requirement for the official CRC-32 standard.
// You can create CRCs without it, but they won't conform to the standard.
// Used only by crc32_init()
static uint32_t crc32_reflect(uint32_t ref, char ch)
{
	uint32_t value = 0;

	// Swap bit 0 for bit 7
	// bit 1 for bit 6, etc.
	for(int i = 1; i < (ch + 1); i++)
	{
		if(ref & 1)
			value |= 1 << (ch - i);
		ref >>= 1;
	}
	return value;
}

void crc32_init()
{
	// Quit if already initialised
	if (g_bInitialized)
		return;

	g_bInitialized = true;

	// This is the official polynomial used by CRC-32
	// in PKZip, WinZip and Ethernet.
	uint32_t ulPolynomial = 0x04c11db7;

	// 256 values representing ASCII character codes.
	for(int i = 0; i <= 0xFF; i++)
	{
		g_dwCRCTable[i]=crc32_reflect(i, 8) << 24;
		for (int j = 0; j < 8; j++)
			g_dwCRCTable[i] = (g_dwCRCTable[i] << 1) ^ (g_dwCRCTable[i] & (1 << 31) ? ulPolynomial : 0);
		g_dwCRCTable[i] = crc32_reflect(g_dwCRCTable[i], 32);
	}
}

void crc32_start(uint32_t* pcrc)
{
	// Make sure initialised
	crc32_init(false);
	*pcrc = 0xFFFFFFFF;
}

void crc32_update_1(uint32_t* pcrc, uint8_t byte)
{
	*pcrc = (*pcrc >> 8) ^ g_dwCRCTable[(*pcrc & 0xFF) ^ byte];
}

void crc32_update(uint32_t* pcrc, const void* pbDataIn, int cbData)
{
	const unsigned char* pbData=(const unsigned char*)pbDataIn;
	while(cbData--)
	{
		*pcrc = (*pcrc >> 8) ^ g_dwCRCTable[(*pcrc & 0xFF) ^ *pbData++];
	}
}

void crc32_finish(uint32_t* pcrc)
{
	// Exclusive OR the result with the beginning value.
	*pcrc = *pcrc ^ 0xffffffff;
}

uint32_t crc32(const void* pbData, int cbData)
{
	uint32_t dwCRC;
	crc32_start(&dwCRC);
	crc32_update(&dwCRC, pbData, cbData);
	crc32_finish(&dwCRC);
	return dwCRC;
}


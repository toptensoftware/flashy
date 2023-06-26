#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void crc32_start(uint32_t* pcrc);
void crc32_update_1(uint32_t* pcrc, uint8_t byte);
void crc32_update(uint32_t* pcrc, const void* pData, int cbData);
void crc32_finish(uint32_t* pcrc);
uint32_t crc32(const void* pbData, int cbData);

#ifdef __cplusplus
}
#endif


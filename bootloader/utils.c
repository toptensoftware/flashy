#include "common.h"

const char* stralloc(const char* psz)
{
    if (psz == NULL)
        return NULL;

    size_t len = strlen(psz) + 1;
    char* pMem = (char*)malloc(len);
    memcpy(pMem, psz, len);
    return pMem;
}
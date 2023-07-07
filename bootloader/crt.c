#include <stddef.h>
#include <string.h>
#include <stdint.h>

void *memset(void *pBuffer, int nValue, size_t nLength)
{
    uint8_t* p =(uint8_t*)pBuffer;
    uint8_t* pEnd = p + nLength;
    while(p < pEnd)
        *p++ =(uint8_t)nValue;
    return pBuffer;
}

void *memcpy(void *pDest, const void *pSrc, size_t nLength)
{
    uint8_t* ps =(uint8_t*)pSrc;
    uint8_t* pd =(uint8_t*)pDest;
    uint8_t* pse = ps + nLength;
    while(ps < pse)
        *pd++ = *ps++;
    return pDest;

}

int memcmp(const void* str1, const void* str2, size_t len)
{
    const unsigned char* a = (const unsigned char*)str1;
    const unsigned char* b = (const unsigned char*)str2;

    const unsigned char* pEnd = a + len;
    while (a < pEnd)
    {
        if (*a != *b)
            return *a - *b;

        a++;
        b++;
    }

    return 0;
}


char* strchr(const char* str, int find)
{
    const char* p = str;
    while (*p)
    {
        if (*p == find)
        {
            return (char*)p;
        }
        p++;
    }
    return 0;
}


char* strcpy(char* dest, const char* src)
{
    char* pd = dest;
    while (*src)
    {
        *pd++ = *src++;
    }

    *pd = '\0';

    return dest;
}


size_t strlen(const char* str)
{
    const char* p = str;
    while (*p)
        p++;
    return p - str;
}


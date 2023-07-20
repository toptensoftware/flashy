#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>

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


char* strncpy(char* dest, const char* src, size_t len)
{
    char* pd = dest;
    const char* pEnd = src + len;
    while (*src && src < pEnd)
    {
        *pd++ = *src++;
    }

    if (src < pEnd)
    {
        *pd = '\0';
    }

    return dest;
}

int strcmp(const char* str1, const char* str2)
{
    const char* a = str1;
    const char* b = str2;
    while (*a && *b)
    {
        if (*a != *b)
            return *a - *b;

        a++;
        b++;
    }

    return *a - *b;
}

size_t strlen(const char* str)
{
    const char* p = str;
    while (*p)
        p++;
    return p - str;
}


int memrev(void* pszStart, void* pszEnd)
{
    // Reverse it
    char* start = (char*)pszStart;
    char* end = (char*)pszEnd - 1;
    while (end > start)
    {
        // swap
        char temp = *start;
        *start = *end;
        *end = temp;

        start++;
        end--;
    }

    // Return the length
    return (int)(pszEnd - pszStart);
}

int abs(int n)
{
	return n < 0 ? -n : n;
}



int ultostr(char* buf, unsigned long value, int base, int uppercase, int padWidth, int prefix)
{
    // Format tnumber
    char* p = buf;
    do
    {
        char ch = (char)(value % base) + '0';
        if (ch > '9')
            ch += (uppercase ? 'A' : 'a') - '9' - 1;
        *p++ = ch;
        value /= base;
    } while (value != 0);

    while (p - buf < padWidth)
    {
        *p++ = '0';
    }

    if (prefix)
    {
        if (base == 8)
            *p++ = '0';
        else if (base == 16)
        {
            *p++ = uppercase ? 'X' : 'x';
            *p++ = '0';
        }
    }

    *p = '\0';

    return memrev(buf, p);
}

int ulltostr(char* buf, unsigned long long value, int base, int uppercase, int padWidth, int prefix)
{
    // Format tnumber
    char* p = buf;
    do
    {
        char ch = (char)(value % base) + '0';
        if (ch > '9')
            ch += (uppercase ? 'A' : 'a') - '9' - 1;
        *p++ = ch;
        value /= base;
    } while (value != 0);

    while (p - buf < padWidth)
    {
        *p++ = '0';
    }

    if (prefix)
    {
        if (base == 8)
            *p++ = '0';
        else if (base == 16)
        {
            *p++ = uppercase ? 'X' : 'x';
            *p++ = '0';
        }
    }

    *p = '\0';

    return memrev(buf, p);
}


int lltostr(char* buf, long long value, int padWidth, char positiveSign)
{
    // Handle negative
    int negative = value < 0;

    // Format tnumber
    char* p = buf;
    do
    {
        *p++ = '0' + (char)abs(value % 10);
        value /= 10;
    } while (value != 0);

    while (p - buf < padWidth)
    {
        *p++ = '0';
    }

    // Put back the negative
    if (negative)
    {
        *p++ = '-';
    }
    else if (positiveSign)
    {
        *p++ = positiveSign;
    }
    *p = '\0';

    return memrev(buf, p);
}

int ltostr(char* buf, long value, int padWidth, char positiveSign)
{
    // Handle negative
    int negative = value < 0;

    // Format tnumber
    char* p = buf;
    do
    {
        *p++ = '0' + (char)abs(value % 10);
        value /= 10;
    } while (value != 0);

    while (p - buf < padWidth)
    {
        *p++ = '0';
    }

    // Put back the negative
    if (negative)
    {
        *p++ = '-';
    }
    else if (positiveSign)
    {
        *p++ = positiveSign;
    }
    *p = '\0';

    return memrev(buf, p);
}

int dtostr_f(char* buf, double value, int precision, char positiveSymbol)
{
    strcpy(buf, "#ERR");
    return 4;
}

int dtostr_e(char* buf, double value, int precision, char positiveSymbol, char expSymbol)
{
    strcpy(buf, "#ERR");
    return 4;
}


static void write_repeat(void (*write)(void*,char), void* arg, char ch, int count)
{
    for (int i = 0; i < count; i++)
        write(arg, ch);
}

static void write_str(void (*write)(void*,char), void* arg, const char* psz, int len)
{
    for (int i = 0; i < len; i++)
        write(arg,psz[i]);
}


static void write_align(void (*write)(void*,char), void* arg, const char* psz, int len, int width, int left)
{
    if (len < 0)
        len = 0;
    if (width > len)
    {
        if (left)
        {
            write_str(write, arg, psz, len);
            write_repeat(write, arg, ' ', width - len);
        }
        else
        {
            write_repeat(write, arg, ' ', width - len);
            write_str(write, arg, psz, len);
        }
    }
    else
    {
        write_str(write, arg, psz, len);
    }
};


// Supported formats:
// %[-][+][ ][#][0][<width>][.<precision>][l|ll]<type>
// [-] = left align
// [+] = include positive sign
// [ ] = display positive sign as a space
// [#] = include '0x' (or '0X') on hex numbers and pointers, '0' on octal numbers
// [0] = pad with leading zeros (if right aligned)
// [l] = long
// [ll] = long long
// [z] = pointer size integer 
// <width> = width as integer or '*' to read from arg list
// <precision> = precision as integer or '*' to read from arg list
// <type> = 'c', 's', 'i', 'd', 'u', 'x', 'X', 'o', 'p', 'f', 'e'
// Note: one of the goals of this method is to provide a sprint style formatter
//       that works exactly the same across platforms.  Old versions of CString
//       called the crt sprintf family of functions but differences like %s %S
//       and so on meant the format string had to be patched or the client had
//       to provide different format strings for different platforms which is a
//       total pain.  This might not be the perfect formatter, but at least it's
//       consistent.

void ffsh_printf(void (*write)(void*, char), void* arg, const char* format, va_list args)
{
    // Temp buffer for formatting numbers into
    // This needs to be so big for formatting doubles where there could
    // be 308 digits in the whole part (e+308), and also a really wide
    // precision that the caller might specify.  The precision will be 
    // clamped to ensure this doesn't overflow, but gives a lot of leeway.
    char szTemp[700];

    // Format string
    const char* p = format;
    while (*p != '\0')
    {
        if (*p == '%')
        {
            p++;

            // escaped '%' with %%?
            if (*p == '%')
            {
                write(arg, '%');
                p++;
                continue;
            }

            // Parse flags
            int bTypePrefix = 0;
            int bLeft = 0;
            char chPositivePrefix = '\0';
            int bZeroPrefix = 0;
            int bLong = 0;
            int bLongLong = 0;
            int bSizeT = 0;

        next_flag:						// sometimes a goto is a friend
            if (*p == '#')
            {
                bTypePrefix = 1;
                p++;
                goto next_flag;
            }

            if (*p == '-')
            {
                bLeft = 1;
                p++;
                goto next_flag;
            }

            if (*p == '+')
            {
                chPositivePrefix = '+';
                p++;
                goto next_flag;
            }
            if (*p == ' ')
            {
                if (chPositivePrefix == '\0')
                    chPositivePrefix = ' ';
                p++;
                goto next_flag;
            }

            // Zero prefix?
            if (*p == '0')
            {
                bZeroPrefix = !bLeft;
                p++;
            }

            // Parse width
            int iWidth = 0;
            if (*p == '*')
            {
                iWidth = va_arg(args, int);
                p++;
            }
            else
            {
                while ('0' <= *p && *p <= '9')
                {
                    iWidth = iWidth * 10 + (*p++ - '0');
                }
            }

            // Parse precision
            int iPrecision = -1;
            if (*p == '.')
            {
                p++;
                bZeroPrefix = 0;

                if (*p == '*')
                {
                    iPrecision = va_arg(args, int);
                    p++;
                }
                else
                {
                    iPrecision = 0;
                    while ('0' <= *p && *p <= '9')
                    {
                        iPrecision = iPrecision * 10 + (*p++ - '0');
                    }
                }

                // Make sure we don't overflow our local number formatting buffer
                if (iPrecision > sizeof(szTemp) - 330)
                    iPrecision = sizeof(szTemp) - 330;
            }

            // Type modifiers 'l' and 'll'
            if (*p == 'l')
            {
                if (p[1] == 'l')
                {
                    bLongLong = 1;
                    p++;
                }
                else
                {
                    bLong = 1;
                }

                p++;
            }

            if (*p == 'z')
            {
                bSizeT = 1;
                p++;
            }

            // Type specifier
            switch (*p)
            {
            case 'c':
            {
                char val = (char)va_arg(args, int);
                write_align(write, arg, &val, 1, iWidth, bLeft);
                p++;
                break;
            }

            case 's':
            {
                const char* val = va_arg(args, const char*);
                char temp[10];
                if (val == NULL)
                {
                    char* d = temp;
                    for (const char* p = "(null)"; *p; d++, p++)
                    {
                        *d = *p;
                    }
                    *d++ = '\0';
                    val = temp;
                }
                int nLen = strlen(val);
                if (nLen > iPrecision && iPrecision > 0)
                    nLen = iPrecision;
                write_align(write, arg, val, nLen, iWidth, bLeft);
                p++;
                break;
            }

            case 'd':
            case 'i':
                if (bZeroPrefix)
                {
                    iPrecision = iWidth;
                    iWidth = 0;
                }

                // Process by type...
                if (bSizeT)
                {
                    ptrdiff_t val = va_arg(args, ptrdiff_t);
                    if (bZeroPrefix && (val < 0 || chPositivePrefix))
                        iPrecision--;
                    write_align(write, arg, szTemp, lltostr(szTemp, val, iPrecision, chPositivePrefix), iWidth, bLeft);
                }
                else if (bLongLong)
                {
                    long long val = va_arg(args, long long);
                    if (bZeroPrefix && (val < 0 || chPositivePrefix))
                        iPrecision--;
                    write_align(write, arg, szTemp, lltostr(szTemp, val, iPrecision, chPositivePrefix), iWidth, bLeft);
                }
                else if (bLong)
                {
                    long val = va_arg(args, long);
                    if (bZeroPrefix && (val < 0 || chPositivePrefix))
                        iPrecision--;
                    write_align(write, arg, szTemp, ltostr(szTemp, val, iPrecision, chPositivePrefix), iWidth, bLeft);
                }
                else
                {
                    int val = va_arg(args, int);
                    if (bZeroPrefix && (val < 0 || chPositivePrefix))
                        iPrecision--;
                    write_align(write, arg, szTemp, ltostr(szTemp, val, iPrecision, chPositivePrefix), iWidth, bLeft);
                }
                p++;
                break;

            case 'u':
            case 'x':
            case 'X':
            case 'o':
            {
                if (bZeroPrefix)
                {
                    iPrecision = iWidth;
                    iWidth = 0;
                }

                // Work out base
                int base = 10;
                if (*p == 'o')
                    base = 8;
                else if (*p == 'x' || *p == 'X')
                    base = 16;

                // Update case version?
                int upper = *p == 'X';

                // Process by type...
                if (bSizeT)
                {
                    size_t val = va_arg(args, size_t);
                    write_align(write, arg, szTemp, ulltostr(szTemp, val, base, upper, iPrecision, bTypePrefix), iWidth, bLeft);
                }
                else if (bLongLong)
                {
                    long long val = va_arg(args, unsigned long long);
                    write_align(write, arg, szTemp, ulltostr(szTemp, val, base, upper, iPrecision, bTypePrefix), iWidth, bLeft);
                }
                else if (bLong)
                {
                    long val = va_arg(args, unsigned long);
                    write_align(write, arg, szTemp, ultostr(szTemp, val, base, upper, iPrecision, bTypePrefix), iWidth, bLeft);
                }
                else
                {
                    int val = va_arg(args, unsigned int);
                    write_align(write, arg, szTemp, ultostr(szTemp, val, base, upper, iPrecision, bTypePrefix), iWidth, bLeft);
                }
                p++;
                break;
            }

            case 'p':
            case 'P':
            {
                // Pointer
                int base = 16;
                int upper = *p == 'P';
                size_t val = va_arg(args, size_t);
                write_align(write, arg, szTemp, ulltostr(szTemp, val, base, upper, sizeof(size_t) * 2, bTypePrefix), iWidth, bLeft);
                p++;
                break;
            }

            case 'f':
            {
                // Floating point, fixed format
                double val = va_arg(args, double);
                write_align(write, arg, szTemp, dtostr_f(szTemp, val, iPrecision < 0 ? 6 : iPrecision, chPositivePrefix), iWidth, bLeft);
                p++;
                break;
            }

            case 'e':
            {
                // Floating point, exponent format
                double val = va_arg(args, double);
                write_align(write, arg, szTemp, dtostr_e(szTemp, val, iPrecision < 0 ? 6 : iPrecision, chPositivePrefix, *p), iWidth, bLeft);
                p++;
                break;
            }

            default:
                // Huh? what was that?
                p++;
                break;
            }
        }
        else
        {
            write(arg, *p++);
        }
    }
}

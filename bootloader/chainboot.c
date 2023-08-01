#include <errno.h>
#include "common.h"

#if AARCH == 32
#define BASE_ADDRESS    (uint32_t)0x8000
#else
#define BASE_ADDRESS    (uint32_t)0x80000
#endif



// Load chain boot image
int load_chain_image_file(const char* filename)
{
    trace("Loading %s\n", filename);

    // Open the file
    FIL file;
    int err = f_open(&file, filename, FA_READ | FA_OPEN_EXISTING);
    if (err)
    {
        return err;
    }

    // Read file
    UINT bytes_read;
    uint8_t* p = (uint8_t*)(size_t)BASE_ADDRESS;
    while (true)
    {
        err = f_read(&file, p, 4096, &bytes_read);
        if (err)
            break;

        if (bytes_read < 4096)
            break;

        p += bytes_read;
    }
    f_close(&file);

    if (err)
        return err;

    return 0;
}

// Get the kernel suffixes for a particular pi model
const char** kernel_suffixes_for_model(int model)
{
    static const char* suffixes_1[] = { "", NULL };
    static const char* suffixes_2[] = { "7", NULL };
    #if AARCH == 32
    static const char* suffixes_3[] = { "7", "8-32", NULL };
    static const char* suffixes_4[] = { "7l", NULL };
    #else
    static const char* suffixes_3[] = { "8", NULL };
    static const char* suffixes_4[] = { "8-rpi4", NULL };
    #endif

    switch (model)
    {
        case 1: return suffixes_1;
        case 2: return suffixes_2;
        case 3: return suffixes_3;
        case 4: return suffixes_4;
    }
    return NULL;
}

int load_chain_image(const char* path)
{
    // Work buffer
    char sz[512];

    // Does it contain '*'
    const char* pszStar = strchr(path, '*');
    if (pszStar)
    {
        // Replace * with kernel suffix for current platform
        const char** ppszSuffixes = kernel_suffixes_for_model(get_major_model());
        if (ppszSuffixes == NULL)
        {
            return -1;
        }

        // Try to find file with matching suffix
        int index = 0;
        while (ppszSuffixes[index])
        {
            // Replace * with suffix
            strncpy(sz, path, pszStar - path);
            sz[pszStar - path] = '\0';
            strcat(sz, ppszSuffixes[index]);
            strcat(sz, pszStar + 1);

            // Try loading it
            int err = load_chain_image_file(sz);
            if (err == 0)
                return 0;

            // Try next
            index++;
        }
        return ENOENT;
    }    

    // Is it a directory?
    FILINFO fi;
    int err = f_stat_ex(path, &fi);
    if (err)
    {
        return err;
    }

    if (fi.fattrib & AM_DIR)
    {
        // Yes, look for "kernel*.img"
        strcpy(sz, path);
        pathcat(sz, "kernel*.img");
        return load_chain_image(sz);
    }
    else
    {
        // No, just load it
        return load_chain_image_file(path);
    }
}


// Run chain boot image
void run_chain_image()
{
    // Boot it
    BRANCHTO(BASE_ADDRESS);
}



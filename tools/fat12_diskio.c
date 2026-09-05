/* PC harness: compile the exact FatFs sources used by bdmfs_fatfs.irx
   against a raw image file, and dump every directory entry name as hex +
   printable text. Verifies the FF_LFN_UNICODE config change end-to-end. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ff.h"
#include "diskio.h"   /* DSTATUS/DRESULT/disk_* declarations (FatFs >= R0.15) */

static FILE *g_img;

DSTATUS disk_initialize(BYTE p)
{
    (void)p;
    g_img = fopen("fat12.img", "rb");
    return g_img ? 0 : STA_NODISK;
}

DSTATUS disk_status(BYTE p)
{
    (void)p;
    return (g_img && !ferror(g_img)) ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE p, BYTE *buff, LBA_t sector, UINT count)
{
    (void)p;
    if (!g_img) return RES_NOTRDY;
    if (fseek(g_img, (long)sector * 512, SEEK_SET) != 0) return RES_ERROR;
    if (fread(buff, 512, count, g_img) != count) return RES_ERROR;
    return RES_OK;
}

DRESULT disk_write(BYTE p, const BYTE *buff, LBA_t sector, UINT count)
{
    (void)p; (void)buff; (void)sector; (void)count;
    return RES_WRPRT;
}

DRESULT disk_ioctl(BYTE p, BYTE cmd, void *buff)
{
    (void)p; (void)cmd; (void)buff;
    return RES_OK;
}

DWORD get_fattime(void)
{
    return ((DWORD)(2026 - 1980) << 25) | ((DWORD)9 << 21) | ((DWORD)4 << 16);
}

/* FF_FS_REENTRANT = 1 in the ps2sdk ffconf, so ff.c references the OS glue
   in ffsystem.c (IOP semaphores). The harness is single-threaded: no-ops. */
int  ff_mutex_create(int vol) { (void)vol; return 1; }
void ff_mutex_delete(int vol) { (void)vol; }
int  ff_mutex_take  (int vol) { (void)vol; return 1; }
void ff_mutex_give  (int vol) { (void)vol; }

int main(void)
{
    FATFS fs;
    DIR dp;
    FILINFO fno;
    FRESULT res;

    res = f_mount(&fs, "0:", 0);
    printf("f_mount -> %d (fs_type=%d)\n", res, fs.fs_type);
    if (res != FR_OK) return 1;

    res = f_opendir(&dp, "0:/");
    printf("f_opendir -> %d\n", res);
    if (res != FR_OK) return 1;

    for (;;)
    {
        const char *p;
        res = f_readdir(&dp, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;

        printf("fname  len=%u hex=", (unsigned)strlen(fno.fname));
        for (p = fno.fname; *p; p++)
            printf("%02X ", (unsigned char)*p);
        printf("\n       txt=[%s]\n", fno.fname);
        if (fno.altname[0])
            printf("altname=[%s]\n", fno.altname);
    }
    printf("done\n");
    return 0;
}

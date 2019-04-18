#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <switch.h>
#include "pmc.h"

#define IRAM(a) ((vu32 *)((a)-0x40000000+iram_base_va))

extern u32 rebootstub_bin[];
extern u32 rebootstub_bin_size;

/* From nx-hbloader main.c */
extern void* __stack_top;//Defined in libnx.
#define STACK_SIZE 0x100000 //Change this if main-thread stack size ever changes.

#define MODULE_NEREBA 999

void __libnx_initheap(void)
{
    static char g_innerheap[0x20000];

    extern char* fake_heap_start;
    extern char* fake_heap_end;

    fake_heap_start = &g_innerheap[0];
    fake_heap_end   = &g_innerheap[sizeof g_innerheap];
}

void __appInit(void)
{
    Result rc;

    rc = smInitialize();
    if (R_FAILED(rc))
        fatalSimple(MAKERESULT(MODULE_NEREBA, 1));

    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        rc = setsysGetFirmwareVersion(&fw);
        if (R_SUCCEEDED(rc))
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();
    }

    rc = fsInitialize();
    if (R_FAILED(rc))
        fatalSimple(MAKERESULT(MODULE_NEREBA, 2));

    fsdevMountSdmc();
}

void __appExit(void)
{
    fsdevUnmountAll();
    fsExit();
    smExit();
}

u64 iram_base_va, pmc_base_va;

static Result fetch_io_regs(void)
{
    Result rc;
    rc = svcQueryIoMapping(&iram_base_va, 0x40000000, 0x40000);
    if(R_FAILED(rc)) return rc;
    rc = svcQueryIoMapping(&pmc_base_va, 0x7000E400, 0xC00);
    return rc;
}

int main(int argc, char **argv)
{
    u32 payload_buf[0x1ED58 / 4]; //max size for RCM payloads is 0x1ED58
    mkdir("sdmc:/nereba", ACCESSPERMS);
    FILE *log_f = fopen("sdmc:/nereba/nereba.log", "w");

    if(!log_f)
        return 0;

    setlinebuf(log_f); //set line buffered mode so the file updates on every newline.

    fprintf(log_f, "Nereba v0.1 by stuckpixel\n");

    Result rc = fetch_io_regs();

    if(rc)
    {
        fprintf(log_f, "Failed to fetch I/0 regions! Errcode: %08x\n", rc);
        fclose(log_f);
        fatalSimple(MAKERESULT(MODULE_NEREBA, 3));
    }

    fprintf(log_f, "Got IO regions!\nCopying stub...\n");

    /* Not using memcpy due to dabort issues (seems 32 bit accesses are enforced on this bus(?)) */
    for(int i = 0; i < rebootstub_bin_size / 4; i++)
    {
        u32 tmp = rebootstub_bin[i];
        *IRAM(0x4003F000 + i * 4) = tmp;
    }

    fprintf(log_f, "Opening nereba.bin\n");
    struct stat file_stat;
    if(stat("sdmc:/nereba/nereba.bin", &file_stat) < 0)
    {
        fprintf(log_f, "Failed to get payload size!\nMake sure nereba.bin is in the nereba folder on the SD card!\n");
        fclose(log_f);
        fatalSimple(MAKERESULT(MODULE_NEREBA, 4));
    }

    FILE *payload_f = fopen("sdmc:/nereba/nereba.bin", "rb");
    if(!payload_f)
    {
        fprintf(log_f, "Failed to open payload!\n");
        fclose(log_f);
        fatalSimple(MAKERESULT(MODULE_NEREBA, 5));
    }

    fprintf(log_f, "Reading payload size %li bytes...\n", file_stat.st_size);
    fread(payload_buf, 1, file_stat.st_size, payload_f);
    fclose(payload_f);

    fprintf(log_f, "Copying payload...\n");
    for(int i = 0; i < file_stat.st_size / 4; i++)
    {
        u32 tmp = payload_buf[i];
        *IRAM(0x40010000 + i * 4) = tmp;
    }

    fprintf(log_f, "Writing scratch regs...\n");

    /* Arbwrite 1 */
    SCRATCH(45) = 0x2E38DFFF; //ipatch: 0x105C70 = 0xDFFF (SVC 0xFF)
    SCRATCH(46) = 0x6001DC28; //overwrite ipatch slot 9

    /* Arbwrite 2 */
    SCRATCH(33) = 0x4003F000; //rebootstub location
    SCRATCH(40) = 0x6000F208; //overwrite BPMP SVC

    fprintf(log_f, "Launching payload now\n");
    fflush(log_f); //flush chages to file
    fclose(log_f); //close file

    /* reboot to payload */
    SCRATCH(0) = 1; // set bit 0 to signify warmboot
    PMC_CNTRL0 = 1 << 4; // Assert MAIN_RST

    while(1);

    return 0;
}

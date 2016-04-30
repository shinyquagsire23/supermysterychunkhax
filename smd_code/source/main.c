#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <3ds.h>

#include "imports.h"
#include "text.h"
#include "pad.h"
#include "stocks.h"

#include "tinyprintf.h"

// http://3dbrew.org/wiki/Nandrw/sys/SecureInfo_A
const char regions[7][4] = {
    "JPN",
    "USA",
    "EUR",
    "EUR",
    "CHN",
    "KOR",
    "TWN"
};

Handle httpcHandle = 0;

Result get_redirect(char *url, char *out, size_t out_size, char *user_agent)
{
    Result ret;

    Handle context;
    ret = _HTTPC_CreateContext(&httpcHandle, url, &context);
    if(ret) return ret;

    Handle httpcHandle2;
    ret = _srvGetServiceHandle(srvHandle, &httpcHandle2, "http:C");
    if(ret) return ret;

    ret = _HTTPC_InitializeConnectionSession(&httpcHandle2, context);
    if(ret) return ret;

    ret = _HTTPC_SetProxyDefault(&httpcHandle2, context);
    if(ret) return ret;

    ret = _HTTPC_AddRequestHeaderField(&httpcHandle2, context, "User-Agent", user_agent);
    if(!ret) ret = _HTTPC_BeginRequest(&httpcHandle2, context);

    if(ret)
    {
        _HTTPC_CloseContext(&httpcHandle2, context);
        return ret;
    }

    ret = _HTTPC_GetResponseHeader(&httpcHandle2, context, "Location", out, out_size);
    _HTTPC_CloseContext(&httpcHandle2, context);

    return ret;
}

Result download_file(Handle service, Handle context, void* buffer, size_t* size)
{
    Result ret;
    int fail = 0;
    char str[256];
    u8* top_framebuffer = &LINEAR_BUFFER[0x00100000];

    ret = _HTTPC_BeginRequest(&service, context);
    if(ret) { fail = -1; goto downloadFail; }

    u32 status_code = 0;
    ret = _HTTPC_GetResponseStatusCode(&service, context, &status_code, 0);
    if(ret) { fail = -2; goto downloadFail; }

    if(status_code != 200) { fail = -3; goto downloadFail; }

    u32 sz = 0;
    ret = _HTTPC_GetDownloadSizeState(&service, context, &sz);
    if(ret)  { fail = -4; goto downloadFail; }

    memset(buffer, 0, sz);

    ret = _HTTPC_ReceiveData(&service, context, buffer, sz);
    if(ret)  { fail = -5; goto downloadFail; }

    if(size) *size = sz;

downloadFail:
    if(fail) sprintf(str, "Failed to download payload %d 0x%lX", fail, ret);
    else sprintf(str, "Successfully downloaded payload!");
    centerString(top_framebuffer, str, 88, 400);

    return ret;
}

Result payload_update(u8* firmware_version)
{
    u8* buffer = &LINEAR_BUFFER[0x00300000];
    size_t size = 0;

    u8* top_framebuffer = &LINEAR_BUFFER[0x00100000];
    char *str = (char*)&top_framebuffer[0x7E900];

    // http://3dbrew.org/wiki/Nandrw/sys/SecureInfo_A
    const char regions[7][4] = {
        "JPN",
        "USA",
        "EUR",
        "EUR",
        "CHN",
        "KOR",
        "TWN"
    };

    char* in_url = str + 128;
    char* out_url = in_url + 512;
    sprintf(in_url, "http://smea.mtheall.com/get_payload.php?version=%s-%d-%d-%d-%d-%s",
                        firmware_version[0] ? "NEW" : "OLD", firmware_version[1], firmware_version[2], firmware_version[3], firmware_version[4], regions[firmware_version[5]]);

    char user_agent[] = "supermysterychunkhax_menu";
    Result ret = get_redirect(in_url, out_url, 512, user_agent);
    if(ret)
    {
        sprintf(str, "get_redirect: 0x%lX", ret);
        centerString(top_framebuffer, str, 40, 400);
        return ret;
    }

    Handle context;
    ret = _HTTPC_CreateContext(&httpcHandle, out_url, &context);
    if(ret) return ret;

    Handle httpcHandle2;
    ret = _srvGetServiceHandle(srvHandle, &httpcHandle2, "http:C");
    if(ret) return ret;

    ret = _HTTPC_InitializeConnectionSession(&httpcHandle2, context);
    if(ret) return ret;

    ret = _HTTPC_SetProxyDefault(&httpcHandle2, context);
    if(ret) return ret;

    ret = download_file(httpcHandle2, context, buffer, &size);
    if(ret) return ret;

    // copy payload to text
    ret = _GSPGPU_FlushDataCache(gspHandle, 0xFFFF8001, (u32*)buffer, (size + 0x1f) & ~0x1f);
    ret = gspwn((void*)(SMD_CODE_LINEAR_BASE + 0x00101000 - 0x00100000), buffer, (size + 0x1f) & ~0x1f);
    svcSleepThread(300*1000*1000);

    FS_Archive save_archive = (FS_Archive){ARCHIVE_SAVEDATA, (FS_Path){PATH_EMPTY, 1, (u8*)""}};
    ret = _FSUSER_OpenArchive(fsHandle, &save_archive);
    if(ret) return ret;

    ret = _FSUSER_DeleteFile(fsHandle, save_archive, _fsMakePath(PATH_ASCII, "/payload.bin"));
    if(ret) return ret;

    ret = _FSUSER_ControlArchive(fsHandle, save_archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
    if(ret) return ret;

    Handle file;
    ret = _FSUSER_OpenFile(fsHandle, &file, 0, 0, save_archive.handle, PATH_ASCII, "/payload.bin", strlen("/payload.bin")+1, FS_OPEN_CREATE | FS_OPEN_WRITE, 0);
    if(ret) return ret;

    u32 btx;
    ret = _FSFILE_Write(file, &btx, 0, (u32*)buffer, size, FS_WRITE_FLUSH | FS_WRITE_UPDATE_TIME);
    if(ret) return ret;

    ret = _FSFILE_Close(file);
    if(ret) return ret;

    ret = _FSUSER_ControlArchive(fsHandle, save_archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
    if(ret) return ret;

    ret = _FSUSER_CloseArchive(fsHandle, &save_archive);
    return ret;
}

void _main()
{
    Result ret = 0x0;

    // un-init DSP so killing SMD will work
    _DSP_UnloadComponent(dspHandle);
    _DSP_RegisterInterruptEvents(dspHandle, 0x0, 0x2, 0x2);

    Handle file;
    ret = _FSUSER_OpenFileDirectly(fsHandle, &file, 0, ARCHIVE_SAVEDATA, PATH_EMPTY, "", 1, PATH_ASCII, "/payload.bin", strlen("/payload.bin")+1, FS_OPEN_READ, 0);
    if(!ret)
    {
        u64 payload_size = 0;
        _FSFILE_GetSize(file, &payload_size);

        u32* payload_buffer = (u32*)&LINEAR_BUFFER[0x00300000];
        u32 btx = 0;
        _FSFILE_Read(file, &btx, 0, payload_buffer, payload_size);

        _FSFILE_Close(file);

        // copy payload to text
        ret = _GSPGPU_FlushDataCache(gspHandle, 0xFFFF8001, payload_buffer, (payload_size + 0x1f) & ~0x1f);
        ret = gspwn((void*)(SMD_CODE_LINEAR_BASE + 0x00101000 - 0x00100000), payload_buffer, (payload_size + 0x1f) & ~0x1f);
        svcSleepThread(300*1000*1000);
    }

    // ghetto dcache invalidation
    // don't judge me
    int i, j;//, k;
    // for(k=0; k<0x2; k++)
        for(j=0; j<0x4; j++)
            for(i=0; i<0x01000000/0x4; i+=0x4)
                LINEAR_BUFFER[i+j]^=0xDEADBABE;

    // put framebuffers in linear mem so they're writable
    u8* top_framebuffer = &LINEAR_BUFFER[0x00100000];
    u8* low_framebuffer = &top_framebuffer[0x00046500];
    _GSPGPU_SetBufferSwap(*gspHandle, 0, (GSPGPU_FramebufferInfo){0, (u32*)top_framebuffer, (u32*)top_framebuffer, 240 * 3, (1<<8)|(1<<6)|1, 0, 0});
    _GSPGPU_SetBufferSwap(*gspHandle, 1, (GSPGPU_FramebufferInfo){0, (u32*)low_framebuffer, (u32*)low_framebuffer, 240 * 3, 1, 0, 0});

    if(padGet() & BUTTON_SELECT)
    {
        clearScreen();

        bool exiting = false;
        u8 state = 0;
        u8 select = 0;
        while(1)
        {
            switch(state)
            {
                case 0:
                    select = 0;
                    centerString(top_framebuffer, "supermysterychunkhax menu", 16, 400);
                    centerString(top_framebuffer, "by Shiny Quagsire and Dazzozo", 24, 400);

                    drawString(top_framebuffer, "Launch *hax payload", 40, 80);
                    drawString(top_framebuffer, "Update *hax payload", 40, 88);
                    drawString(top_framebuffer, "Clear savegame", 40, 96);

                    while(1)
                    {
                        drawStringColor(top_framebuffer, ">", 24, 80+(8*0), select == 0 ? 0x00FF00 : 0x0);
                        drawStringColor(top_framebuffer, ">", 24, 80+(8*1), select == 1 ? 0x00FF00 : 0x0);
                        drawStringColor(top_framebuffer, ">", 24, 80+(8*2), select == 2 ? 0x00FF00 : 0x0);

                        u32 input = padWait();
                        if(input & BUTTON_DOWN && select < 2)
                            select++;
                        if(input & BUTTON_UP && select > 0)
                            select--;
                        if(input & BUTTON_A)
                        {
                            state = select + 1;
                            clearScreen();
                            break;
                        }
                    }
                    break;
                case 1: //Launch
                    clearScreen();
                    state = 0;
                    exiting = true;
                    break;
                case 2: //Update
                    centerString(top_framebuffer, "supermysterychunkhax menu", 16, 400);
                    centerString(top_framebuffer, "by Shiny Quagsire and Dazzozo", 24, 400);

                    centerString(top_framebuffer, "Update *hax payload", 48, 400);

                    u32 input;
                    int firmware_selected_value = 0;
                    u8 firmware_version[6] = {0};
                    char *str = (char*)&top_framebuffer[0x7E900];
                    ret = _srvGetServiceHandle(srvHandle, &httpcHandle, "http:C");
                    if(!ret) ret = _HTTPC_Initialize(&httpcHandle);
                    if(ret)
                    {
                        centerString(top_framebuffer, "Failed to initialize httpc.", 48, 400);
                        goto updateFail;
                    }

                    firmware_version[0] = 0;
                    firmware_version[5] = 1;
                    firmware_version[1] = 9;
                    firmware_version[2] = 0;
                    firmware_version[3] = 0;
                    firmware_version[4] = 7;

                    while(1)
                    {
                        input = padWait();

                        if(input & KEY_LEFT) firmware_selected_value--;
                        if(input & KEY_RIGHT) firmware_selected_value++;

                        if(firmware_selected_value < 0) firmware_selected_value = 0;
                        if(firmware_selected_value > 5) firmware_selected_value = 5;

                        if(input & KEY_UP) firmware_version[firmware_selected_value]++;
                        if(input & KEY_DOWN) firmware_version[firmware_selected_value]--;

                        int firmware_maxnum = 256;
                        if(firmware_selected_value == 0) firmware_maxnum = 2;
                        if(firmware_selected_value == 5) firmware_maxnum = 7;

                        if(firmware_version[firmware_selected_value] < 0) firmware_version[firmware_selected_value] = 0;
                        if(firmware_version[firmware_selected_value] >= firmware_maxnum) firmware_version[firmware_selected_value] = firmware_maxnum - 1;

                        if(input & KEY_A) break;

                        char *str_top = (firmware_version[firmware_selected_value] < firmware_maxnum - 1) ? "^" : "-";
                        char *str_bottom = (firmware_version[firmware_selected_value] > 0) ? "v" : "-";

                        int offset = 26;
                        if(firmware_selected_value)
                        {
                            offset += 6;

                            for(int i = 1; i < firmware_selected_value; i++)
                            {
                                offset += 2;
                                if(firmware_version[i] >= 10) offset++;
                            }

                            if(firmware_selected_value >= 5)
                            {
                                offset += 1;
                            }
                        }

                        //Black out selection arrows
                        drawStringColor(top_framebuffer, "                    ", 26*8, 56, 0x000000);
                        drawStringColor(top_framebuffer, "                    ", 26*8, 72, 0x000000);

                        sprintf(str, "    Selected firmware: %s %d-%d-%d-%d %s  \n", firmware_version[0] ? "New3DS" : "Old3DS", firmware_version[1], firmware_version[2], firmware_version[3], firmware_version[4], regions[firmware_version[5]]);
                        drawString(top_framebuffer, str, 16, 64);

                        drawStringColor(top_framebuffer, str_top, offset * 8, 56, 0x00FF00);
                        drawStringColor(top_framebuffer, str_bottom, offset * 8, 72, 0x00FF00);
                    }

                    //Black out selection arrows
                    drawStringColor(top_framebuffer, "                    ", 26*8, 56, 0x000000);
                    drawStringColor(top_framebuffer, "                    ", 26*8, 72, 0x000000);

                    ret = payload_update(firmware_version);
                    if(!ret)
                        centerString(top_framebuffer, "Successfully updated payload!", 96, 400);
                    else
                    {
                        sprintf(str, "Failed to update payload: 0x%lX", ret);
                        centerString(top_framebuffer, str, 96, 400);
                    }

                    drawString(top_framebuffer, "Press B to return.", 24, 122);

updateFail:
                    while(1)
                    {
                        input = padWait();
                        if(input & BUTTON_B)
                            break;
                    }
                    clearScreen();
                    state = 0;
                    break;
                case 3: //Clear save
                    centerString(top_framebuffer, "supermysterychunkhax menu", 16, 400);
                    centerString(top_framebuffer, "by Shiny Quagsire and Dazzozo", 24, 400);
                    char *buf = (char*)&top_framebuffer[0x7E900];

                    centerString(top_framebuffer, "Clearing savegame...", 40, 400);

                    FS_Archive save_archive = (FS_Archive){ARCHIVE_SAVEDATA, (FS_Path){PATH_EMPTY, 1, (u8*)""}};
                    ret = _FSUSER_FormatSaveData(fsHandle, save_archive.id, save_archive.lowPath, 512, 10, 10, 11, 11, true);
                    hex2str(buf, ret);

                    if(!ret)
                        centerString(top_framebuffer, "Savegame cleared!", 48, 400);
                    else
                    {
                        centerString(top_framebuffer, "Format failed. :(", 48, 400);
                        centerString(top_framebuffer, buf, 56, 400);
                    }

                    drawString(top_framebuffer, "Press B to return.", 24, 72);

                    while(1)
                    {
                        u32 input = padWait();
                        if(input & BUTTON_B)
                            break;
                    }
                    clearScreen();
                    state = 0;
                    break;
            }


            if(exiting)
                break;
        }
    }

    // run payload
    {
        void (*payload)(u32* paramlk, u32* stack_pointer) = (void*)0x00101000;
        u32* paramblk = (u32*)LINEAR_BUFFER;

        paramblk[0x1c >> 2] = SMD_GSPGPU_GXCMD4;
        paramblk[0x20 >> 2] = SMD_GSPGPU_FLUSHDATACACHE_WRAPPER;
        paramblk[0x48 >> 2] = 0x8d; // flags
        paramblk[0x58 >> 2] = SMD_GSPGPU_HANDLE;
        paramblk[0x64 >> 2] = 0x08010000;

        payload(paramblk, (u32*)(0x10000000 - 4));
    }

    *(u32*)ret = 0xdead0008;
}

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#include <3ds.h>

Handle game_fs_session;
FS_Archive save_archive, sdmc_archive;

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

typedef enum
{
    STATE_NONE,
    STATE_INITIALIZE,
    STATE_INITIAL,
    STATE_SELECT_FIRMWARE,
    STATE_DOWNLOAD_PAYLOAD,
    STATE_INSTALL_PAYLOAD,
    STATE_INSTALLED_PAYLOAD,
    STATE_ERROR,
} state_t;

char status[256];

Result get_redirect(char *url, char *out, size_t out_size, char *user_agent)
{
    Result ret;

    httpcContext context;
    ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 0);
    if(R_FAILED(ret)) return ret;

    ret = httpcAddRequestHeaderField(&context, "User-Agent", user_agent);
    if(R_SUCCEEDED(ret)) ret = httpcBeginRequest(&context);

    if(R_FAILED(ret))
    {
        httpcCloseContext(&context);
        return ret;
    }

    ret = httpcGetResponseHeader(&context, "Location", out, out_size);
    httpcCloseContext(&context);

    return 0;
}

Result download_file(httpcContext *context, void** buffer, size_t* size)
{
    Result ret;

    ret = httpcBeginRequest(context);
    if(R_FAILED(ret)) return ret;

    u32 status_code = 0;
    ret = httpcGetResponseStatusCode(context, &status_code, 0);
    if(R_FAILED(ret)) return ret;

    if(status_code != 200) return -1;

    u32 sz = 0;
    ret = httpcGetDownloadSizeState(context, NULL, &sz);
    if(R_FAILED(ret)) return ret;

    void* buf = malloc(sz);
    if(!buf) return -2;

    memset(buf, 0, sz);

    ret = httpcDownloadData(context, buf, sz, NULL);
    if(R_FAILED(ret))
    {
        free(buf);
        return ret;
    }

    if(size) *size = sz;
    if(buffer) *buffer = buf;
    else free(buf);

    return 0;
}

Result write_savedata(const char* path, const void* data, size_t size)
{
    if(!path || !data || size == 0) return -1;

    Result ret;
    int fail = 0;

    fsUseSession(game_fs_session, false);

    Handle file = 0;
    ret = FSUSER_OpenFile(&file, save_archive, fsMakePath(PATH_ASCII, path), FS_OPEN_CREATE | FS_OPEN_WRITE, 0);
    if(R_FAILED(ret))
    {
        fail = -1;
        goto writeFail;
    }

    u32 bytes_written = 0;
    ret = FSFILE_Write(file, &bytes_written, 0, data, size, FS_WRITE_FLUSH | FS_WRITE_UPDATE_TIME);
    if(R_FAILED(ret))
    {
        fail = -2;
        goto writeFail;
    }

    ret = FSFILE_Close(file);
    if(R_FAILED(ret))
    {
        fail = -3;
        goto writeFail;
    }

    ret = FSUSER_ControlArchive(save_archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
    if(R_FAILED(ret)) fail = -4;

writeFail:
    fsEndUseSession();
    if(fail) sprintf(status, "Failed to write to file: %d\n     %08lX %08lX", fail, ret, bytes_written);
    else sprintf(status, "Successfully wrote to file!\n     %08lX               ", bytes_written);

    return ret;
}

Result do_install(const void* payload, size_t payload_size, u64 program_id, bool new3ds)
{
    Result ret = 0;
    int fail = 0;

    if(!payload || payload_size == 0)
    {
        fail = -1;
        goto installFail;
    }

    char save_dn[256];
    snprintf(save_dn, sizeof(save_dn) - 1, "romfs:/%016llx/%s", program_id, new3ds ? "New3DS" : "Old3DS");

    char save_fn[256];
    snprintf(save_fn, sizeof(save_fn) - 1, "%s/%s", save_dn, "game_header");

    FILE* f = fopen(save_fn, "rb");
    if(!f)
    {
        fail = -2;
        goto installFail;
    }

    int fd = fileno(f);
    if(fd == -1)
    {
        fclose(f);
        fail = -3;
        goto installFail;
    }

    struct stat filestats;
    if(fstat(fd, &filestats) == -1)
    {
        fclose(f);
        fail = -4;
        goto installFail;
    }

    size_t save_size = filestats.st_size;
    if(save_size == 0)
    {
        fclose(f);
        fail = -5;
        goto installFail;
    }

    u32* save_buffer = malloc(save_size);
    if(!save_buffer)
    {
        fclose(f);
        fail = -6;
        goto installFail;
    }

    size_t bytes_read = fread(save_buffer, 1, save_size, f);
    fclose(f);
    if(bytes_read != save_size)
    {
        free(save_buffer);
        fail = -7;
        goto installFail;
    }

    // check that this save is the otherapp loader
    if(save_buffer[0x8000-2] != 0xF00F0001)
    {
        free(save_buffer);
        fail = -8;
        goto installFail;
    }

    // copy otherapp payload and update size
    memcpy(&save_buffer[0x8000], payload, payload_size);
    save_buffer[0x8000-1] = payload_size;

    // format the savegame and mount it
    fsUseSession(game_fs_session, false);
    ret = FSUSER_FormatSaveData(save_archive.id, save_archive.lowPath, 0x200, 10, 10, 11, 11, true);
    if(R_SUCCEEDED(ret)) ret = FSUSER_OpenArchive(&save_archive);
    fsEndUseSession();

    if(R_FAILED(ret))
    {
        free(save_buffer);
        fail = -9;
        goto installFail;
    }

    ret = write_savedata("/game_header", save_buffer, save_size);
    free(save_buffer);

    if(R_FAILED(ret)) return ret;

installFail:
    if(fail) sprintf(status, "Failed to install payload: %d\n     %08lX", fail, ret);
    else sprintf(status, "Successfully installed payload!\n                            ");

    return ret;
}

int main()
{
    gfxInitDefault();
    gfxSet3D(false);

    PrintConsole topConsole, botConsole;
    consoleInit(GFX_TOP, &topConsole);
    consoleInit(GFX_BOTTOM, &botConsole);

    consoleSelect(&topConsole);
    consoleClear();

    state_t current_state = STATE_NONE;
    state_t next_state = STATE_INITIALIZE;

    static char top_text[2048];
    char top_text_tmp[256];
    top_text[0] = '\0';

    int firmware_version[6] = {0};
    int firmware_selected_value = 0;

    void* payload_buffer = NULL;
    size_t payload_size = 0;

    u64 program_id = 0;

    while(aptMainLoop())
    {
        hidScanInput();
        if(hidKeysDown() & KEY_START) break;

        // transition function
        if(next_state != current_state)
        {
            memset(top_text_tmp, 0, sizeof(top_text_tmp));

            switch(next_state)
            {
                case STATE_INITIALIZE:
                    strncat(top_text, "Initializing... You may press START at any time\nto return to menu.\n\n", sizeof(top_text) - 1);
                    break;
                case STATE_INITIAL:
                    strncat(top_text, "Welcome to the supermysterychunkhax installer!\nPlease proceed with caution, as you might lose\ndata if you don't.\n\nPress A to continue.\n\n", sizeof(top_text) - 1);
                    break;
                case STATE_SELECT_FIRMWARE:
                    strncat(top_text, "Please select your console's firmware version.\nOnly select NEW 3DS if you own a New 3DS (XL).\nD-Pad to select, A to continue.\n", sizeof(top_text) - 1);
                    break;
                case STATE_DOWNLOAD_PAYLOAD:
                    snprintf(top_text, sizeof(top_text) - 1, "%s\n\n\nDownloading payload...\n", top_text);
                    break;
                case STATE_INSTALL_PAYLOAD:
                    strncat(top_text, "Installing payload...\n\n", sizeof(top_text) - 1);
                    break;
                case STATE_INSTALLED_PAYLOAD:
                    snprintf(top_text_tmp, sizeof(top_text_tmp) - 1, "Done!\nsupermysterychunkhax was successfully installed.");
                    break;
                case STATE_ERROR:
                    strncat(top_text, "Looks like something went wrong. :(\n", sizeof(top_text) - 1);
                    break;
                default:
                    break;
            }

            if(top_text_tmp[0]) strncat(top_text, top_text_tmp, sizeof(top_text) - 1);

            current_state = next_state;
        }

        consoleSelect(&topConsole);
        printf("\x1b[0;%dHsupermysterychunkhax_installer", (50 - 31) / 2);
        printf("\x1b[1;%dHby Shiny Quagsire and Dazzozo\n\n\n", (50 - 30) / 2);
        printf(top_text);

        // state function
        switch(current_state)
        {
            case STATE_INITIALIZE:
                {
                    sdmc_archive = (FS_Archive){ARCHIVE_SDMC, (FS_Path){PATH_EMPTY, 1, (u8*)""}};
                    save_archive = (FS_Archive){ARCHIVE_SAVEDATA, (FS_Path){PATH_EMPTY, 1, (u8*)""}};

                    // already done by __appInit() but we explicitly want fs so..
                    fsInit();
                    FSUSER_OpenArchive(&sdmc_archive);

                    // get an fs:USER session as the game
                    Result ret = srvGetServiceHandleDirect(&game_fs_session, "fs:USER");
                    if(R_SUCCEEDED(ret)) ret = FSUSER_Initialize(game_fs_session);
                    if(R_FAILED(ret))
                    {
                        snprintf(status, sizeof(status) - 1, "Failed to get game fs:USER session.\n    Error code: %08lX", ret);
                        next_state = STATE_ERROR;
                        break;
                    }

                    ret = httpcInit(0);
                    if(R_FAILED(ret))
                    {
                        snprintf(status, sizeof(status) - 1, "Failed to initialize httpc.\n    Error code: %08lX", ret);
                        next_state = STATE_ERROR;
                        break;
                    }

                    OS_VersionBin nver_versionbin, cver_versionbin;
                    ret = osGetSystemVersionData(&nver_versionbin, &cver_versionbin);
                    if(R_FAILED(ret))
                    {
                        snprintf(status, sizeof(status) - 1, "Failed to get the system version.\n    Error code: %08lX", ret);
                        next_state = STATE_ERROR;
                        break;
                    }

                    ret = cfguInit();
                    if(R_FAILED(ret))
                    {
                        snprintf(status, sizeof(status) - 1, "Failed to initialize cfgu.\n    Error code: %08lX", ret);
                        next_state = STATE_ERROR;
                        break;
                    }

                    u8 region = 0;
                    ret = CFGU_SecureInfoGetRegion(&region);
                    if(R_FAILED(ret))
                    {
                        snprintf(status, sizeof(status) - 1, "Failed to get the system region.\n    Error code: %08lX", ret);
                        next_state = STATE_ERROR;
                        break;
                    }

                    cfguExit();

                    u8 is_new3ds = 0;
                    APT_CheckNew3DS(&is_new3ds);

                    firmware_version[0] = is_new3ds;
                    firmware_version[5] = region;

                    firmware_version[1] = cver_versionbin.mainver;
                    firmware_version[2] = cver_versionbin.minor;
                    firmware_version[3] = cver_versionbin.build;
                    firmware_version[4] = nver_versionbin.mainver;

                    aptOpenSession();
                    ret = APT_GetProgramID(&program_id);
                    aptCloseSession();

                    if(R_FAILED(ret))
                    {
                        snprintf(status, sizeof(status) - 1, "Failed to get the program ID for the current process.\n    Error code: %08lX", ret);
                        next_state = STATE_ERROR;
                        break;
                    }

                    static const u64 tids[] = {
                        0x0004000000149b00,
                        0x0004000000174400,
                        0x0004000000174600
                    };
                    for(int i = 0; i < sizeof(tids) / sizeof(u64); i++)
                    {
                        if(program_id == tids[i])
                        {
                            ret = 0;
                            break;
                        }
                        else
                            ret = -1;
                    }

                    if(R_FAILED(ret))
                    {
                        snprintf(status, sizeof(status) - 1, "Running title is not Pokemon SMD.\nCheck that the installer XML is on your SD card.");
                        next_state = STATE_ERROR;
                        break;
                    }

                    u64 program_id_update = 0;
                    if(((program_id >> 32) & 0xFFFF) == 0) program_id_update = program_id | 0x0000000E00000000ULL;

                    if(program_id_update)
                    {
                        ret = amInit();
                        if(R_FAILED(ret))
                        {
                            snprintf(status, sizeof(status) - 1, "Failed to initialize AM.\n    Error code: %08lX", ret);
                            next_state = STATE_ERROR;
                            break;
                        }

                        AM_TitleEntry title_entry;
                        ret = AM_GetTitleInfo(1, 1, &program_id_update, &title_entry);
                        amExit();
                        if(R_SUCCEEDED(ret))
                        {
                            // doesn't exist (yet)
                            snprintf(status, sizeof(status) - 1, "An update title exists and is currently unsupported.");
                            next_state = STATE_ERROR;
                            break;
                        }
                    }

                    ret = romfsInit();
                    if(R_FAILED(ret))
                    {
                        snprintf(status, sizeof(status) - 1, "Failed to initialize romfs for this application (romfsInit()).\n    Error code: %08lX", ret);
                        next_state = STATE_ERROR;
                        break;
                    }

                    next_state = STATE_INITIAL;
                }
                break;

            case STATE_INITIAL:
                if(hidKeysDown() & KEY_A) next_state = STATE_SELECT_FIRMWARE;
                break;

            case STATE_SELECT_FIRMWARE:
                {
                    if(hidKeysDown() & KEY_LEFT) firmware_selected_value--;
                    if(hidKeysDown() & KEY_RIGHT) firmware_selected_value++;

                    if(firmware_selected_value < 0) firmware_selected_value = 0;
                    if(firmware_selected_value > 5) firmware_selected_value = 5;

                    if(hidKeysDown() & KEY_UP) firmware_version[firmware_selected_value]++;
                    if(hidKeysDown() & KEY_DOWN) firmware_version[firmware_selected_value]--;

                    int firmware_maxnum = 256;
                    if(firmware_selected_value == 0) firmware_maxnum = 2;
                    if(firmware_selected_value == 5) firmware_maxnum = 7;

                    if(firmware_version[firmware_selected_value] < 0) firmware_version[firmware_selected_value] = 0;
                    if(firmware_version[firmware_selected_value] >= firmware_maxnum) firmware_version[firmware_selected_value] = firmware_maxnum - 1;

                    if(hidKeysDown() & KEY_A) next_state = STATE_DOWNLOAD_PAYLOAD;

                    int offset = 26;
                    if(firmware_selected_value)
                    {
                        offset += 7;

                        for(int i = 1; i < firmware_selected_value; i++)
                        {
                            offset += 2;
                            if(firmware_version[i] >= 10) offset++;
                        }
                    }

                    printf((firmware_version[firmware_selected_value] < firmware_maxnum - 1) ? "%*s^%*s" : "%*s-%*s", offset, " ", 50 - offset - 1, " ");
                    printf("      Selected firmware: %s %d-%d-%d-%d %s  \n", firmware_version[0] ? "New3DS" : "Old3DS", firmware_version[1], firmware_version[2], firmware_version[3], firmware_version[4], regions[firmware_version[5]]);
                    printf((firmware_version[firmware_selected_value] > 0) ? "%*sv%*s" : "%*s-%*s", offset, " ", 50 - offset - 1, " ");
                }
                break;

            case STATE_DOWNLOAD_PAYLOAD:
                {
                    httpcContext context;
                    static char in_url[512];
                    static char out_url[512];

                    sprintf(in_url, "http://smea.mtheall.com/get_payload.php?version=%s-%d-%d-%d-%d-%s",
                        firmware_version[0] ? "NEW" : "OLD", firmware_version[1], firmware_version[2], firmware_version[3], firmware_version[4], regions[firmware_version[5]]);

                    char user_agent[] = "supermysterychunkhax_installer";
                    Result ret = get_redirect(in_url, out_url, 512, user_agent);
                    if(R_FAILED(ret))
                    {
                        sprintf(status, "Failed to grab payload url\n    Error code: %08lX", ret);
                        next_state = STATE_ERROR;
                        break;
                    }

                    ret = httpcOpenContext(&context, HTTPC_METHOD_GET, out_url, 0);
                    if(R_FAILED(ret))
                    {
                        sprintf(status, "Failed to open http context\n    Error code: %08lX", ret);
                        next_state = STATE_ERROR;
                        break;
                    }

                    ret = download_file(&context, &payload_buffer, &payload_size);
                    if(R_FAILED(ret))
                    {
                        sprintf(status, "Failed to download payload\n    Error code: %08lX", ret);
                        next_state = STATE_ERROR;
                        break;
                    }

                    next_state = STATE_INSTALL_PAYLOAD;
                }
                break;

            case STATE_INSTALL_PAYLOAD:
                {
                    Result ret = do_install(payload_buffer, payload_size, program_id, firmware_version[0]);

                    if(R_FAILED(ret))
                    {
                        next_state = STATE_ERROR;
                        break;
                    }

                    next_state = STATE_INSTALLED_PAYLOAD;
                }
                break;

            case STATE_INSTALLED_PAYLOAD:
                next_state = STATE_NONE;
                break;

            default: break;
        }

        consoleSelect(&botConsole);
        printf("\x1b[0;0H  Current status:\n    %s\n", status);

        gspWaitForVBlank();
    }

    romfsExit();
    httpcExit();

    fsUseSession(game_fs_session, false);
    FSUSER_CloseArchive(&save_archive);
    fsEndUseSession();
    svcCloseHandle(game_fs_session);

    FSUSER_CloseArchive(&sdmc_archive);
    fsExit();

    gfxExit();
    return 0;
}

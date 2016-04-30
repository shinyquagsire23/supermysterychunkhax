#ifndef _STOCKS_H
#define _STOCKS_H

#include <3ds.h>
#include "../../constants/constants.h"

Result _srvGetServiceHandle(Handle* srvHandle, Handle* out, const char* name);

Result gspwn(void* dst, void* src, u32 size);
Result _GSPGPU_SetBufferSwap(Handle handle, u32 screenid, GSPGPU_FramebufferInfo framebufinfo);
Result _FSUSER_OpenArchive(Handle* handle, FS_Archive* archive);
//Result _FSUSER_OpenFile(Handle* handle, Handle* out, FS_Archive archive, FS_Path fileLowPath, u32 openflags, u32 attributes);
//Result _FSUSER_OpenFileDirectly(Handle* handle, Handle* out, FS_Archive archive, FS_Path fileLowPath, u32 openflags, u32 attributes);
Result _FSUSER_DeleteFile(Handle *handle, FS_Archive archive, FS_Path fileLowPath);
Result _FSFILE_Read(Handle handle, u32 *bytesRead, u64 offset, u32 *buffer, u32 size);
Result _FSFILE_GetSize(Handle handle, u64 *size);
Result _FSFILE_Write(Handle handle, u32 *bytesWritten, u64 offset, u32 *data, u32 size, u32 flushFlags);
Result _FSFILE_Close(Handle handle);
Result _FSUSER_OpenArchive(Handle *handle, FS_Archive *archive);
Result _FSUSER_CloseArchive(Handle* handle, FS_Archive* archive);
Result _FSUSER_ControlArchive(Handle *handle, FS_Archive archive, FS_ArchiveAction action, void* input, u32 inputSize, void* output, u32 outputSize);
Result _FSUSER_FormatSaveData(Handle* handle, FS_ArchiveID archiveId, FS_Path path, u32 blocks, u32 directories, u32 files, u32 directoryBuckets, u32 fileBuckets, bool duplicateData);
FS_Path _fsMakePath(FS_PathType type, const void* path);

Result _HTTPC_Initialize(Handle* handle);
Result _HTTPC_CreateContext(Handle* handle, char* url, Handle* contextHandle);
Result _HTTPC_InitializeConnectionSession(Handle* handle, Handle contextHandle);
Result _HTTPC_SetProxyDefault(Handle* handle, Handle contextHandle);
Result _HTTPC_CloseContext(Handle* handle, Handle contextHandle);
Result _HTTPC_BeginRequest(Handle* handle, Handle contextHandle);
Result _HTTPC_ReceiveData(Handle* handle, Handle contextHandle, u8* buffer, u32 size);
Result _HTTPC_GetDownloadSizeState(Handle* handle, Handle contextHandle, u32* totalSize);
Result _HTTPC_AddRequestHeaderField(Handle* handle, Handle contextHandle, char* name, char* value);
Result _HTTPC_GetResponseStatusCode(Handle* handle, Handle contextHandle, u32* out, u64 delay);
Result _HTTPC_GetResponseHeader(Handle* handle, Handle contextHandle, char* name, char* value, u32 valuebuf_maxsize);

int _strlen(const char* str);
void *memset(void * ptr, int value, size_t num);

#endif

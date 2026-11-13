/* SPDX-License-Identifier: MIT */
#ifndef FILE_TRACE_STUDIO_CSDK_H
#define FILE_TRACE_STUDIO_CSDK_H
#include <stdint.h>
#if !defined(_WIN32) || !defined(_WIN64)
#error This SDK requires Windows x64.
#endif
#ifdef __cplusplus
#define FILE_TRACE_STUDIO_CSDK_NOEXCEPT noexcept
extern "C" {
#else
#define FILE_TRACE_STUDIO_CSDK_NOEXCEPT
#endif
#if defined(FILE_TRACE_STUDIO_CSDK_BUILD)
#define FILE_TRACE_STUDIO_CSDK_API __declspec(dllexport)
#else
#define FILE_TRACE_STUDIO_CSDK_API __declspec(dllimport)
#endif
/* open is inert. One active session per loaded product DLL.
   call accepts length-delimited UTF-8 JSON and returns an allocated UTF-8
   response, or NULL on allocation failure. Always free with this same DLL.
   Session/operation handles are scoped to the client that returned them. */
FILE_TRACE_STUDIO_CSDK_API uint32_t file_trace_studio_json_open(uint64_t *session) FILE_TRACE_STUDIO_CSDK_NOEXCEPT;
FILE_TRACE_STUDIO_CSDK_API uint32_t file_trace_studio_json_close(uint64_t session) FILE_TRACE_STUDIO_CSDK_NOEXCEPT;
FILE_TRACE_STUDIO_CSDK_API char *file_trace_studio_json_call(uint64_t session,const char *request,uint32_t length) FILE_TRACE_STUDIO_CSDK_NOEXCEPT;
FILE_TRACE_STUDIO_CSDK_API void file_trace_studio_json_free(char *response) FILE_TRACE_STUDIO_CSDK_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef FILE_TRACE_STUDIO_CSDK_NOEXCEPT
#endif

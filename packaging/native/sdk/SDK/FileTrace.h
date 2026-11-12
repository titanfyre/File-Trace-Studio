#pragma once
#if defined(_KERNEL_MODE)
typedef ULONG FT_U32;
typedef LONG FT_I32;
typedef USHORT FT_U16;
typedef ULONGLONG FT_U64;
#else
#include <stdint.h>
typedef uint32_t FT_U32;
typedef int32_t FT_I32;
typedef uint16_t FT_U16;
typedef uint64_t FT_U64;
#endif

/* Public wire format. No pointers, file names or file contents cross this boundary. */
#define FT_VERSION 1u
#define FT_READ_LIMIT 64u
#define FT_RING_CAPACITY 1024u
#define FT_CONTEXT_LIMIT 256u
#define FT_PORT_NAME L"\\TitanFileTracePort"
#define FT_COMMAND_STATUS 1u
#define FT_COMMAND_START 2u
#define FT_COMMAND_STOP 3u
#define FT_COMMAND_READ 4u
#define FT_FLAG_IRP 1u
#define FT_FLAG_FAST_IO 2u

typedef struct FT_COMMAND {
    FT_U32 Version, Size, Operation, ProcessId;
} FT_COMMAND;

typedef struct FT_EVENT {
    FT_U64 Sequence, StartTicks, DurationTicks, RequestedBytes, TransferredBytes;
    FT_U32 ProcessId;
    FT_I32 Status;
    FT_U16 Operation, Flags;
    FT_U32 Reserved;
} FT_EVENT;

typedef struct FT_HEADER {
    FT_U32 Version, Size, Count, Capacity, ActiveProcessId, Inflight;
    FT_U64 Dropped, Frequency, Epoch, NextSequence;
} FT_HEADER;

typedef struct FT_REPLY {
    FT_HEADER Header;
    FT_EVENT Events[FT_READ_LIMIT];
} FT_REPLY;

#ifdef __cplusplus
static_assert(sizeof(FT_COMMAND)==16, "Wire command size");
static_assert(sizeof(FT_EVENT)==56, "Wire event size");
static_assert(sizeof(FT_HEADER)==56, "Wire header size");
static_assert(sizeof(FT_REPLY)==3640, "Wire reply size");
#endif

#ifndef _KERNEL_MODE
#pragma once

#include <Windows.h>
#include <string>
#include <vector>

namespace FileTrace {
struct Batch { FT_HEADER status{}; std::vector<FT_EVENT> events; };
class Client {
    HANDLE port_=INVALID_HANDLE_VALUE;
    DWORD Request(uint32_t operation,uint32_t pid,Batch& batch);
public:
    Client()=default;
    ~Client();
    Client(const Client&)=delete;
    Client& operator=(const Client&)=delete;
    DWORD Connect();
    DWORD Start(uint32_t pid,Batch& batch);
    DWORD Stop(Batch& batch);
    DWORD Read(Batch& batch);
    DWORD Status(Batch& batch);
    void Close() noexcept;
};
// Decoder is separately testable: rejects malformed/uninitialized records.
DWORD Decode(const void* bytes,size_t size,Batch& batch);
double Microseconds(const FT_EVENT& event,uint64_t frequency) noexcept;
const char* Operation(uint16_t operation) noexcept;
std::string Error(DWORD code);
Batch Sample(); // Synthetic offline example; never connects to the driver.
}

#endif

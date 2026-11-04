/* Original read-only minifilter. Private source; not part of the public MIT SDK. */
#include <fltKernel.h>
#include "FileTrace.h"
C_ASSERT(sizeof(FT_COMMAND)==16);
C_ASSERT(sizeof(FT_EVENT)==56);
C_ASSERT(sizeof(FT_HEADER)==56);
C_ASSERT(sizeof(FT_REPLY)==3640);

#define FT_TAG 'rTfF'
typedef struct FT_CONTEXT {
    ULONGLONG Epoch, Start, Requested;
    ULONG Pid;
    USHORT Major, Flags;
} FT_CONTEXT;
typedef struct FT_STATE {
    PFLT_FILTER Filter;
    PFLT_PORT ServerPort, ClientPort;
    KSPIN_LOCK Lock;
    NPAGED_LOOKASIDE_LIST Contexts;
    EX_RUNDOWN_REF Session;
    EX_RUNDOWN_REF Lifecycle;
    volatile LONG ActivePid, Inflight, Connected, Messages;
    PEPROCESS Process;
    ULONGLONG Epoch, NextSequence, Dropped, Frequency;
    ULONG Head, Count;
    FT_EVENT Ring[FT_RING_CAPACITY];
} FT_STATE;
static FT_STATE g;

static VOID StopCapture(VOID)
{
    PEPROCESS old;KIRQL irql;
    KeAcquireSpinLock(&g.Lock,&irql);
    InterlockedExchange(&g.ActivePid,0);old=g.Process;g.Process=NULL;
    ++g.Epoch;g.Head=0;g.Count=0;
    KeReleaseSpinLock(&g.Lock,irql);
    if(old)ObDereferenceObject(old);
}

static FLT_PREOP_CALLBACK_STATUS PreOperation(PFLT_CALLBACK_DATA Data,PCFLT_RELATED_OBJECTS Objects,PVOID* CompletionContext)
{
    KIRQL irql;ULONGLONG epoch;ULONG pid,activePid;FT_CONTEXT* context;USHORT flags=0;
    UNREFERENCED_PARAMETER(Objects);*CompletionContext=NULL;
    // Acquire load has no locked read-modify-write on x64. Unselected I/O avoids the ring lock.
    activePid=(ULONG)ReadAcquire(&g.ActivePid);
    if(!activePid||FltGetRequestorProcessId(Data)!=activePid)return FLT_PREOP_SUCCESS_NO_CALLBACK;
    KeAcquireSpinLock(&g.Lock,&irql);
    if(!g.Process||FltGetRequestorProcess(Data)!=g.Process){KeReleaseSpinLock(&g.Lock,irql);return FLT_PREOP_SUCCESS_NO_CALLBACK;}
    if(g.Inflight>=FT_CONTEXT_LIMIT){++g.Dropped;KeReleaseSpinLock(&g.Lock,irql);return FLT_PREOP_SUCCESS_NO_CALLBACK;}
    InterlockedIncrement(&g.Inflight);epoch=g.Epoch;pid=(ULONG)g.ActivePid;
    KeReleaseSpinLock(&g.Lock,irql);
    context=ExAllocateFromNPagedLookasideList(&g.Contexts);
    if(!context){
        KeAcquireSpinLock(&g.Lock,&irql);if(g.Epoch==epoch)++g.Dropped;KeReleaseSpinLock(&g.Lock,irql);
        InterlockedDecrement(&g.Inflight);return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }
    if(FLT_IS_IRP_OPERATION(Data))flags|=FT_FLAG_IRP;
    if(FLT_IS_FASTIO_OPERATION(Data))flags|=FT_FLAG_FAST_IO;
    context->Epoch=epoch;context->Pid=pid;context->Major=Data->Iopb->MajorFunction;context->Flags=flags;context->Requested=0;
    if(context->Major==IRP_MJ_READ)context->Requested=Data->Iopb->Parameters.Read.Length;
    else if(context->Major==IRP_MJ_WRITE)context->Requested=Data->Iopb->Parameters.Write.Length;
    context->Start=(ULONGLONG)KeQueryPerformanceCounter(NULL).QuadPart;
    *CompletionContext=context;return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

static FLT_POSTOP_CALLBACK_STATUS PostOperation(PFLT_CALLBACK_DATA Data,PCFLT_RELATED_OBJECTS Objects,PVOID CompletionContext,FLT_POST_OPERATION_FLAGS Flags)
{
    FT_CONTEXT* context=CompletionContext;KIRQL irql;FT_EVENT event={0};ULONGLONG end;
    UNREFERENCED_PARAMETER(Objects);
    if(!(Flags&FLTFL_POST_OPERATION_DRAINING)){
        end=(ULONGLONG)KeQueryPerformanceCounter(NULL).QuadPart;
        event.StartTicks=context->Start;event.DurationTicks=end>=context->Start?end-context->Start:0;
        event.ProcessId=context->Pid;event.Operation=context->Major;event.Flags=context->Flags;
        event.RequestedBytes=context->Requested;event.Status=Data->IoStatus.Status;
        if((context->Major==IRP_MJ_READ||context->Major==IRP_MJ_WRITE)&&NT_SUCCESS(Data->IoStatus.Status))
            event.TransferredBytes=min((ULONGLONG)Data->IoStatus.Information,context->Requested);
        KeAcquireSpinLock(&g.Lock,&irql);
        if(g.ActivePid&&g.Epoch==context->Epoch){
            event.Sequence=g.NextSequence++;
            if(g.Count<FT_RING_CAPACITY){g.Ring[(g.Head+g.Count)%FT_RING_CAPACITY]=event;++g.Count;}
            else ++g.Dropped;
        }
        KeReleaseSpinLock(&g.Lock,irql);
    }
    ExFreeToNPagedLookasideList(&g.Contexts,context);InterlockedDecrement(&g.Inflight);
    return FLT_POSTOP_FINISHED_PROCESSING;
}

static NTSTATUS InstanceSetup(PCFLT_RELATED_OBJECTS Objects,FLT_INSTANCE_SETUP_FLAGS Flags,DEVICE_TYPE Type,FLT_FILESYSTEM_TYPE Fs)
{
    UNREFERENCED_PARAMETER(Objects);UNREFERENCED_PARAMETER(Flags);
    return Type==FILE_DEVICE_DISK_FILE_SYSTEM&&(Fs==FLT_FSTYPE_NTFS||Fs==FLT_FSTYPE_REFS)?STATUS_SUCCESS:STATUS_FLT_DO_NOT_ATTACH;
}
static NTSTATUS InstanceQueryTeardown(PCFLT_RELATED_OBJECTS Objects,FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags)
{UNREFERENCED_PARAMETER(Objects);UNREFERENCED_PARAMETER(Flags);return STATUS_SUCCESS;}

static NTSTATUS Connect(PFLT_PORT Port,PVOID ServerCookie,PVOID ConnectionContext,ULONG Size,PVOID* Cookie)
{
    UNREFERENCED_PARAMETER(ServerCookie);UNREFERENCED_PARAMETER(ConnectionContext);
    if(Size)return STATUS_ACCESS_DENIED;
    KeEnterCriticalRegion();
    if(!ExAcquireRundownProtection(&g.Lifecycle)){KeLeaveCriticalRegion();return STATUS_FLT_DELETING_OBJECT;}
    if(InterlockedCompareExchange(&g.Connected,1,0)){
        ExReleaseRundownProtection(&g.Lifecycle);KeLeaveCriticalRegion();return STATUS_ACCESS_DENIED;
    }
    ExInitializeRundownProtection(&g.Session);StopCapture();g.ClientPort=Port;*Cookie=&g;
    ExReleaseRundownProtection(&g.Lifecycle);KeLeaveCriticalRegion();return STATUS_SUCCESS;
}
static VOID Disconnect(PVOID Cookie)
{
    UNREFERENCED_PARAMETER(Cookie);ExWaitForRundownProtectionRelease(&g.Session);
    StopCapture();FltCloseClientPort(g.Filter,&g.ClientPort);InterlockedExchange(&g.Connected,0);
}

static NTSTATUS MessageInner(PVOID Cookie,PVOID Input,ULONG InputLength,PVOID Output,ULONG OutputLength,PULONG Returned)
{
    FT_COMMAND command;FT_REPLY* reply;NTSTATUS status=STATUS_SUCCESS;KIRQL irql;
    ULONG count=0,bytes;PEPROCESS process=NULL,old=NULL;
    UNREFERENCED_PARAMETER(Cookie);*Returned=0;
    if(!Input||InputLength!=sizeof(command)||!Output||OutputLength<sizeof(FT_HEADER))return STATUS_INVALID_PARAMETER;
    __try{RtlCopyMemory(&command,Input,sizeof(command));}
    __except(FsRtlIsNtstatusExpected(GetExceptionCode())?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return GetExceptionCode();}
    if(command.Version!=FT_VERSION||command.Size!=sizeof(command)||command.Operation<FT_COMMAND_STATUS||command.Operation>FT_COMMAND_READ||
       (command.Operation==FT_COMMAND_START?!command.ProcessId:command.ProcessId!=0))return STATUS_INVALID_PARAMETER;
    if(command.Operation==FT_COMMAND_START){
        status=PsLookupProcessByProcessId(ULongToHandle(command.ProcessId),&process);if(!NT_SUCCESS(status))return status;
    }
    // This staging buffer is filled while the ring spin lock is held: it must be nonpaged.
    reply=ExAllocatePool2(POOL_FLAG_NON_PAGED,sizeof(*reply),FT_TAG);
    if(!reply){if(process)ObDereferenceObject(process);return STATUS_INSUFFICIENT_RESOURCES;}
    RtlZeroMemory(reply,sizeof(*reply));
    KeAcquireSpinLock(&g.Lock,&irql);
    if(command.Operation==FT_COMMAND_START||command.Operation==FT_COMMAND_STOP){
        old=g.Process;g.Process=process;process=NULL;++g.Epoch;g.Head=0;g.Count=0;g.Dropped=0;g.NextSequence=1;
        InterlockedExchange(&g.ActivePid,command.Operation==FT_COMMAND_START?(LONG)command.ProcessId:0);
    }
    if(command.Operation==FT_COMMAND_READ){
        count=min(g.Count,min(FT_READ_LIMIT,(OutputLength-(ULONG)sizeof(FT_HEADER))/(ULONG)sizeof(FT_EVENT)));
        for(ULONG i=0;i<count;++i)reply->Events[i]=g.Ring[(g.Head+i)%FT_RING_CAPACITY];
        g.Head=(g.Head+count)%FT_RING_CAPACITY;g.Count-=count;
    }
    reply->Header.Version=FT_VERSION;reply->Header.Size=sizeof(FT_HEADER)+count*sizeof(FT_EVENT);
    reply->Header.Count=count;reply->Header.Capacity=FT_RING_CAPACITY;reply->Header.ActiveProcessId=(ULONG)g.ActivePid;
    reply->Header.Inflight=(ULONG)g.Inflight;reply->Header.Dropped=g.Dropped;reply->Header.Frequency=g.Frequency;
    reply->Header.Epoch=g.Epoch;reply->Header.NextSequence=g.NextSequence;
    KeReleaseSpinLock(&g.Lock,irql);
    if(old)ObDereferenceObject(old);
    bytes=reply->Header.Size;
    __try{RtlCopyMemory(Output,reply,bytes);*Returned=bytes;}
    __except(FsRtlIsNtstatusExpected(GetExceptionCode())?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){status=GetExceptionCode();KeAcquireSpinLock(&g.Lock,&irql);if(g.Epoch==reply->Header.Epoch)g.Dropped+=count;KeReleaseSpinLock(&g.Lock,irql);}
    ExFreePoolWithTag(reply,FT_TAG);return status;
}
static NTSTATUS Message(PVOID Cookie,PVOID Input,ULONG InputLength,PVOID Output,ULONG OutputLength,PULONG Returned)
{
    NTSTATUS status;*Returned=0;
    KeEnterCriticalRegion();
    if(!ExAcquireRundownProtection(&g.Lifecycle)){KeLeaveCriticalRegion();return STATUS_FLT_DELETING_OBJECT;}
    if(!ExAcquireRundownProtection(&g.Session)){ExReleaseRundownProtection(&g.Lifecycle);KeLeaveCriticalRegion();return STATUS_PORT_DISCONNECTED;}
    if(InterlockedIncrement(&g.Messages)>4){
        InterlockedDecrement(&g.Messages);ExReleaseRundownProtection(&g.Session);ExReleaseRundownProtection(&g.Lifecycle);KeLeaveCriticalRegion();return STATUS_DEVICE_BUSY;
    }
    status=MessageInner(Cookie,Input,InputLength,Output,OutputLength,Returned);
    InterlockedDecrement(&g.Messages);ExReleaseRundownProtection(&g.Session);ExReleaseRundownProtection(&g.Lifecycle);KeLeaveCriticalRegion();return status;
}

static NTSTATUS Unload(FLT_FILTER_UNLOAD_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Flags);FltCloseCommunicationPort(g.ServerPort);
    ExWaitForRundownProtectionRelease(&g.Lifecycle);StopCapture();FltUnregisterFilter(g.Filter);
    NT_ASSERT(g.Inflight==0);ExDeleteNPagedLookasideList(&g.Contexts);return STATUS_SUCCESS;
}
static const FLT_OPERATION_REGISTRATION Operations[]={
    {IRP_MJ_CREATE,0,PreOperation,PostOperation},
    {IRP_MJ_READ,FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,PreOperation,PostOperation},
    {IRP_MJ_WRITE,FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,PreOperation,PostOperation},
    {IRP_MJ_SET_INFORMATION,0,PreOperation,PostOperation},
    {IRP_MJ_CLEANUP,0,PreOperation,PostOperation},
    {IRP_MJ_OPERATION_END}
};
static const FLT_REGISTRATION Registration={
    sizeof(FLT_REGISTRATION),FLT_REGISTRATION_VERSION,0,NULL,Operations,Unload,
    InstanceSetup,InstanceQueryTeardown,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL
};
DRIVER_INITIALIZE DriverEntry;
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject,PUNICODE_STRING RegistryPath)
{
    NTSTATUS status;PSECURITY_DESCRIPTOR security=NULL;OBJECT_ATTRIBUTES attributes;UNICODE_STRING name;
    LARGE_INTEGER frequency;
    UNREFERENCED_PARAMETER(RegistryPath);RtlZeroMemory(&g,sizeof(g));KeInitializeSpinLock(&g.Lock);ExInitializeRundownProtection(&g.Lifecycle);
    ExInitializeNPagedLookasideList(&g.Contexts,NULL,NULL,POOL_NX_ALLOCATION,sizeof(FT_CONTEXT),FT_TAG,0);
    KeQueryPerformanceCounter(&frequency);g.Frequency=(ULONGLONG)frequency.QuadPart;g.Epoch=1;g.NextSequence=1;
    status=FltRegisterFilter(DriverObject,&Registration,&g.Filter);if(!NT_SUCCESS(status))goto fail;
    // Filter Manager's default descriptor grants only SYSTEM and Administrators.
    status=FltBuildDefaultSecurityDescriptor(&security,FLT_PORT_ALL_ACCESS);if(!NT_SUCCESS(status))goto unregister;
    RtlInitUnicodeString(&name,FT_PORT_NAME);
    InitializeObjectAttributes(&attributes,&name,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,NULL,security);
    status=FltCreateCommunicationPort(g.Filter,&g.ServerPort,&attributes,NULL,Connect,Disconnect,Message,1);
    FltFreeSecurityDescriptor(security);if(!NT_SUCCESS(status))goto unregister;
    status=FltStartFiltering(g.Filter);if(NT_SUCCESS(status))return status;
    FltCloseCommunicationPort(g.ServerPort);
unregister:ExWaitForRundownProtectionRelease(&g.Lifecycle);StopCapture();FltUnregisterFilter(g.Filter);
fail:ExDeleteNPagedLookasideList(&g.Contexts);return status;
}

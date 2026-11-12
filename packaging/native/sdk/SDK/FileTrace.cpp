#include "FileTrace.h"
#include <fltuser.h>
#include <algorithm>
#include <cstring>
#include <new>

namespace FileTrace {
namespace { DWORD Win32(HRESULT value){return HRESULT_FACILITY(value)==FACILITY_WIN32?HRESULT_CODE(value):static_cast<DWORD>(value);} }
Client::~Client(){Close();}
void Client::Close() noexcept {if(port_!=INVALID_HANDLE_VALUE){CloseHandle(port_);port_=INVALID_HANDLE_VALUE;}}
DWORD Client::Connect(){
    if(port_!=INVALID_HANDLE_VALUE)return ERROR_SUCCESS;
    HANDLE candidate=INVALID_HANDLE_VALUE;
    HRESULT result=FilterConnectCommunicationPort(FT_PORT_NAME,0,nullptr,0,nullptr,&candidate);
    if(FAILED(result))return Win32(result);
    port_=candidate;return ERROR_SUCCESS;
}
DWORD Client::Request(uint32_t operation,uint32_t pid,Batch& batch){
    batch={};if(port_==INVALID_HANDLE_VALUE)return ERROR_INVALID_HANDLE;
    FT_COMMAND command{FT_VERSION,sizeof(FT_COMMAND),operation,pid};
    FT_REPLY reply{};DWORD returned=0;
    HRESULT result=FilterSendMessage(port_,&command,sizeof(command),&reply,sizeof(reply),&returned);
    if(FAILED(result))return Win32(result);
    return Decode(&reply,returned,batch);
}
DWORD Client::Start(uint32_t pid,Batch& batch){if(!pid){batch={};return ERROR_INVALID_PARAMETER;}return Request(FT_COMMAND_START,pid,batch);}
DWORD Client::Stop(Batch& batch){return Request(FT_COMMAND_STOP,0,batch);}
DWORD Client::Read(Batch& batch){return Request(FT_COMMAND_READ,0,batch);}
DWORD Client::Status(Batch& batch){return Request(FT_COMMAND_STATUS,0,batch);}
DWORD Decode(const void* bytes,size_t size,Batch& batch)try{
    batch={};if(!bytes||size<sizeof(FT_HEADER)||size>sizeof(FT_REPLY))return ERROR_INVALID_DATA;
    FT_HEADER header{};std::memcpy(&header,bytes,sizeof(header));
    if(header.Version!=FT_VERSION||header.Size!=size||header.Count>FT_READ_LIMIT||
       size!=sizeof(FT_HEADER)+header.Count*sizeof(FT_EVENT)||header.Capacity!=FT_RING_CAPACITY||
       !header.Frequency||header.Inflight>FT_CONTEXT_LIMIT||!header.Epoch||!header.NextSequence||(header.Count&&!header.ActiveProcessId))return ERROR_INVALID_DATA;
    Batch result;result.status=header;result.events.resize(header.Count);
    if(header.Count)std::memcpy(result.events.data(),static_cast<const unsigned char*>(bytes)+sizeof(header),header.Count*sizeof(FT_EVENT));
    uint64_t previous=0;
    for(const auto& event:result.events){
        if(event.Reserved||!event.Sequence||event.Sequence<=previous||event.Sequence>=header.NextSequence||
           !event.ProcessId||event.ProcessId!=header.ActiveProcessId||(event.Flags!=FT_FLAG_IRP&&event.Flags!=FT_FLAG_FAST_IO)||
           (event.Operation!=0&&event.Operation!=3&&event.Operation!=4&&event.Operation!=6&&event.Operation!=18)||
           (event.Operation!=3&&event.Operation!=4&&(event.RequestedBytes||event.TransferredBytes))||
           (event.Status<0&&event.TransferredBytes)||event.TransferredBytes>event.RequestedBytes||
           event.DurationTicks>UINT64_MAX-event.StartTicks)return ERROR_INVALID_DATA;
        previous=event.Sequence;
    }
    batch=std::move(result);return ERROR_SUCCESS;
}catch(const std::bad_alloc&){batch={};return ERROR_NOT_ENOUGH_MEMORY;}
double Microseconds(const FT_EVENT& event,uint64_t frequency) noexcept {return frequency?static_cast<double>(event.DurationTicks)*1000000.0/static_cast<double>(frequency):0.0;}
const char* Operation(uint16_t value) noexcept {switch(value){case 0:return "CREATE";case 3:return "READ";case 4:return "WRITE";case 6:return "SET_INFORMATION";case 18:return "CLEANUP";default:return "UNKNOWN";}}
std::string Error(DWORD code){
    if(code==ERROR_FILE_NOT_FOUND||code==ERROR_PATH_NOT_FOUND)return "File Trace driver is not running. Its allocated-altitude and Microsoft-signed package is required for live capture.";
    if(code==ERROR_ACCESS_DENIED)return "Administrator access is required, or another capture client owns the port.";
    if(code==ERROR_INVALID_DATA)return "The driver reply is invalid or incompatible with this SDK.";
    char* text=nullptr;DWORD count=FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,code,MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),reinterpret_cast<char*>(&text),0,nullptr);
    std::string message=count&&text?std::string(text,count):"Windows error "+std::to_string(code);if(text)LocalFree(text);
    while(!message.empty()&&(message.back()=='\r'||message.back()=='\n'))message.pop_back();return message;
}
Batch Sample(){
    Batch batch;batch.status={FT_VERSION,sizeof(FT_HEADER)+4*sizeof(FT_EVENT),4,FT_RING_CAPACITY,4242,0,0,10000000,1,5};
    batch.events={{1,100000,1000,0,0,4242,0,0,FT_FLAG_IRP,0},{2,110000,2300,4096,4096,4242,0,3,FT_FLAG_IRP,0},{3,120000,420,128,0,4242,static_cast<int32_t>(0xC0000022u),4,FT_FLAG_IRP,0},{4,130000,110,0,0,4242,0,18,FT_FLAG_IRP,0}};
    return batch;
}
}

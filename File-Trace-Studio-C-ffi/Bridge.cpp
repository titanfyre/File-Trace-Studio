#include "SDK/FileTrace.h"
#include "WireTypes.h"
#include <map>
struct ProductSession{

FileTrace::Client client;
Json Batch(const FileTrace::Batch& b){return {{"status",EncodeWire(b.status)},{"events",VectorWire(b.events)}};}

Json Invoke(const std::string& op,const Json& a){

if(op=="check"){auto bytes=Bytes(a.at("data"),sizeof(FT_REPLY));FileTrace::Batch b;CheckCode(FileTrace::Decode(bytes.data(),bytes.size(),b));return Batch(b);}
if(op=="sample")return Batch(FileTrace::Sample());
if(op=="connect"){CheckCode(client.Connect());return Json::object();}
if(op=="close"){client.Close();return Json::object();}
FileTrace::Batch batch;
if(op=="start"){CheckCode(client.Start(Number(a,"pid"),batch));return Batch(batch);}
if(op=="stop"){CheckCode(client.Stop(batch));return Batch(batch);}
if(op=="read"){CheckCode(client.Read(batch));return Batch(batch);}
if(op=="status"){CheckCode(client.Status(batch));return Batch(batch);}

throw SdkError(50,"Unknown operation");}
};
static Runtime<ProductSession> runtime;
extern "C" __declspec(dllexport) unsigned file_trace_studio_json_open(std::uint64_t* id)noexcept{return runtime.Open(id);}
extern "C" __declspec(dllexport) unsigned file_trace_studio_json_close(std::uint64_t id)noexcept{return runtime.Close(id);}
extern "C" __declspec(dllexport) char* file_trace_studio_json_call(std::uint64_t id,const char* request,std::uint32_t length)noexcept{return runtime.Call(id,request,length);}
extern "C" __declspec(dllexport) void file_trace_studio_json_free(char* response)noexcept{delete[] response;}

#include "Model.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <iostream>
#include <fstream>
#include <mutex>
#include <random>
#include <thread>

namespace {
size_t checks=0;
void Check(bool condition,const char* why){++checks;if(!condition)throw std::runtime_error(why);}
FT_REPLY Wire(){auto sample=FileTrace::Sample();FT_REPLY reply{};reply.Header=sample.status;std::memcpy(reply.Events,sample.events.data(),sample.events.size()*sizeof(FT_EVENT));return reply;}
// Semantic concurrency model: demonstrates expected stop/epoch policy, not driver execution.
class EpochModel {
    std::mutex lock;uint64_t epoch=1;bool active=false;size_t pending=0,accepted=0;
public:
    uint64_t Start(){std::lock_guard<std::mutex> guard(lock);active=true;accepted=0;return ++epoch;}
    uint64_t Begin(){std::lock_guard<std::mutex> guard(lock);if(!active||pending==FT_CONTEXT_LIMIT)return 0;++pending;return epoch;}
    void Complete(uint64_t token){if(!token)return;std::lock_guard<std::mutex> guard(lock);if(active&&token==epoch)++accepted;--pending;}
    void Stop(){std::lock_guard<std::mutex> guard(lock);active=false;++epoch;}
    size_t Pending(){std::lock_guard<std::mutex> guard(lock);return pending;}
    size_t Accepted(){std::lock_guard<std::mutex> guard(lock);return accepted;}
};
}
int main(){try{
    FileTrace::Batch decoded;FT_REPLY reply=Wire();
    Check(!FileTrace::Decode(&reply,reply.Header.Size,decoded),"valid wire batch");Check(decoded.events.size()==4,"event count");
    Check(std::abs(FileTrace::Microseconds(decoded.events[1],decoded.status.Frequency)-230.0)<1e-10,"QPC units");
    Check(FileTrace::Microseconds(decoded.events[1],0)==0,"zero frequency helper");
    for(size_t size=0;size<sizeof(FT_HEADER);++size)Check(FileTrace::Decode(&reply,size,decoded)==ERROR_INVALID_DATA,"truncated header");
    Check(FileTrace::Decode(nullptr,sizeof(reply),decoded)==ERROR_INVALID_DATA,"null payload");
    auto bad=[&](FT_REPLY damaged){Check(FileTrace::Decode(&damaged,damaged.Header.Size<=sizeof(damaged)?damaged.Header.Size:sizeof(damaged),decoded)==ERROR_INVALID_DATA,"malformed frame rejected");Check(decoded.events.empty(),"failure clears previous data");};
    reply=Wire();reply.Header.Version=2;bad(reply);reply=Wire();reply.Header.Size++;bad(reply);
    reply=Wire();reply.Header.Count=65;bad(reply);reply=Wire();reply.Header.Capacity=0;bad(reply);
    reply=Wire();reply.Header.Frequency=0;bad(reply);reply=Wire();reply.Header.Inflight=257;bad(reply);
    reply=Wire();reply.Header.Epoch=0;bad(reply);reply=Wire();reply.Header.NextSequence=0;bad(reply);
    reply=Wire();reply.Events[0].Reserved=1;bad(reply);reply=Wire();reply.Events[0].ProcessId=0;bad(reply);
    reply=Wire();reply.Events[0].Flags=128;bad(reply);reply=Wire();reply.Events[0].Operation=254;bad(reply);
    reply=Wire();reply.Events[0].Flags=0;bad(reply);reply=Wire();reply.Events[0].ProcessId=21;bad(reply);
    reply=Wire();reply.Header.ActiveProcessId=0;bad(reply);
    reply=Wire();reply.Events[0].Sequence=0;bad(reply);reply=Wire();reply.Events[1].Sequence=1;bad(reply);
    reply=Wire();reply.Events[0].Sequence=5;bad(reply);reply=Wire();reply.Events[0].TransferredBytes=1;bad(reply);
    reply=Wire();reply.Events[2].TransferredBytes=1;bad(reply);reply=Wire();reply.Events[1].TransferredBytes=4097;bad(reply);
    reply=Wire();reply.Events[0].DurationTicks=UINT64_MAX;bad(reply);
    Studio::Model model;auto batch=FileTrace::Sample();model.Add(batch);
    Check(model.total.count==4&&model.total.errors==1&&model.total.bytes==4096,"summary counts/errors/bytes");
    Check(model.operations[1].count==1&&model.operations[2].errors==1,"per-operation mapping");
    for(int i=0;i<500;++i)model.Add(batch);Check(model.history.size()==120,"bounded visualization history");
    batch.status.Epoch++;model.Add(batch);Check(model.total.count==4,"new epoch clears summary");
    wchar_t temp[MAX_PATH]{};DWORD tempLength=GetTempPathW(MAX_PATH,temp);Check(tempLength&&tempLength<MAX_PATH,"test temporary directory");
    std::wstring report=std::wstring(temp)+L"FileTrace-offline-test-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".json";
    Check(Studio::Export(report,model,true)==ERROR_SUCCESS,"local synthetic report export");
    Check(Studio::Export(report,model,true)==ERROR_FILE_EXISTS,"report export never overwrites an existing file");
    std::ifstream input(report);std::string contents((std::istreambuf_iterator<char>(input)),{});input.close();
    Check(contents.find("\"synthetic\":true")!=std::string::npos&&contents.find("\"events\":4")!=std::string::npos,"report preserves synthetic scope and event counts");
    Check(DeleteFileW(report.c_str())!=FALSE,"remove only owned test report");
    uint32_t pid=999;Check(Studio::ParsePid(L"4294967295",pid)&&pid==UINT32_MAX,"PID upper bound");
    for(auto text:{L"",L"0",L"-1",L"1 ",L" 1",L"4294967296",L"12x",L"+12"})Check(!Studio::ParsePid(text,pid),"strict PID parsing");
    std::mt19937 random(1975);std::vector<unsigned char> storage(sizeof(FT_REPLY));
    for(int i=0;i<50000;++i){size_t length=random()%(storage.size()+1);for(size_t j=0;j<length;++j)storage[j]=static_cast<unsigned char>(random());DWORD error=FileTrace::Decode(storage.data(),length,decoded);Check(error==ERROR_INVALID_DATA,"bounded malformed decoder fuzz");}
    EpochModel lifetime;lifetime.Start();uint64_t old=lifetime.Begin();lifetime.Stop();lifetime.Start();lifetime.Complete(old);Check(lifetime.Accepted()==0&&lifetime.Pending()==0,"stale completion after restart ignored and released");
    std::vector<uint64_t> pending;for(unsigned i=0;i<FT_CONTEXT_LIMIT;++i)pending.push_back(lifetime.Begin());Check(lifetime.Begin()==0,"context bound");lifetime.Stop();for(auto token:pending)lifetime.Complete(token);Check(lifetime.Pending()==0,"stop drains bounded contexts");
    lifetime.Start();std::atomic_bool go=false;std::thread completions([&]{while(!go.load())std::this_thread::yield();for(int i=0;i<20000;++i){auto token=lifetime.Begin();lifetime.Complete(token);}});go=true;
    for(int i=0;i<2000;++i){lifetime.Stop();lifetime.Start();}completions.join();lifetime.Stop();Check(lifetime.Pending()==0,"concurrent stop/start semantic rundown");
    std::cout<<checks<<" assertions passed. Offline protocol/model tests only; no driver was loaded.\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAILED: "<<error.what()<<"\n";return 1;}}

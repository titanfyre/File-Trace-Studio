#include "Model.h"
#include <algorithm>
#include <charconv>
#include <iomanip>
#include <sstream>

namespace Studio {
namespace {
uint64_t Add(uint64_t a,uint64_t b){return b>UINT64_MAX-a?UINT64_MAX:a+b;}
void Record(Summary& summary,const FT_EVENT& event){summary.count=Add(summary.count,1);summary.errors=Add(summary.errors,event.Status<0?1:0);summary.bytes=Add(summary.bytes,event.TransferredBytes);summary.ticks+=event.DurationTicks;summary.maximum=(std::max)(summary.maximum,event.DurationTicks);}
}
void Model::Clear(){*this=Model{};}
void Model::Add(const FileTrace::Batch& batch){
    if(status.Epoch&&status.Epoch!=batch.status.Epoch)Clear();
    status=batch.status;
    for(const auto& event:batch.events){
        last=event;hasLast=true;
        size_t index=event.Operation==0?0:event.Operation==3?1:event.Operation==4?2:event.Operation==6?3:4;
        Record(total,event);Record(operations[index],event);
        history.push_back(FileTrace::Microseconds(event,status.Frequency));if(history.size()>120)history.pop_front();
    }
}
double Model::MeanMicroseconds(const Summary& value)const{return value.count&&status.Frequency?static_cast<double>(value.ticks/value.count*1000000.0L/status.Frequency):0.0;}
bool ParsePid(const std::wstring& value,uint32_t& pid){
    pid=0;if(value.empty()||value.size()>10)return false;
    uint64_t parsed=0;for(wchar_t c:value){if(c<L'0'||c>L'9')return false;parsed=parsed*10+static_cast<unsigned>(c-L'0');if(parsed>UINT32_MAX)return false;}
    pid=static_cast<uint32_t>(parsed);return pid!=0;
}
DWORD Export(const std::wstring& path,const Model& model,bool synthetic){
    if(path.empty())return ERROR_INVALID_PARAMETER;
    std::ostringstream json;json.imbue(std::locale::classic());json<<std::setprecision(10);
    json<<"{\n  \"schema\":1,\n  \"synthetic\":"<<(synthetic?"true":"false")<<",\n  \"scope\":\"Observed pre-to-post intervals at this filter; not disk latency or a complete I/O trace\",\n  \"pid\":"<<model.status.ActiveProcessId<<",\n  \"events\":"<<model.total.count<<",\n  \"errors\":"<<model.total.errors<<",\n  \"dropped\":"<<model.status.Dropped<<",\n  \"mean_us\":"<<model.MeanMicroseconds(model.total)<<",\n  \"operations\":[\n";
    constexpr uint16_t codes[]={0,3,4,6,18};
    for(size_t i=0;i<model.operations.size();++i){const auto& s=model.operations[i];json<<"    {\"operation\":\""<<FileTrace::Operation(codes[i])<<"\",\"events\":"<<s.count<<",\"errors\":"<<s.errors<<",\"successful_bytes\":"<<s.bytes<<",\"mean_us\":"<<model.MeanMicroseconds(s)<<"}"<<(i+1==model.operations.size()?"\n":",\n");}
    json<<"  ]\n}\n";auto content=json.str();
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return GetLastError();DWORD written=0;
    bool ok=WriteFile(file,content.data(),static_cast<DWORD>(content.size()),&written,nullptr)&&written==content.size();DWORD error=ok?ERROR_SUCCESS:GetLastError();
    if(!ok&&error==ERROR_SUCCESS)error=ERROR_WRITE_FAULT;CloseHandle(file);return error;
}
}

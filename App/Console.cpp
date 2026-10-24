#include "Model.h"
#include <iostream>
#include <string>

int wmain(int argc,wchar_t** argv){
    if(argc==2&&std::wstring(argv[1])==L"--sample"){
        auto batch=FileTrace::Sample();for(const auto& event:batch.events)std::cout<<FileTrace::Operation(event.Operation)<<"  "<<FileTrace::Microseconds(event,batch.status.Frequency)<<" us  status=0x"<<std::hex<<static_cast<uint32_t>(event.Status)<<std::dec<<"\n";
        std::cout<<"Synthetic offline data. No driver connection was made.\n";return 0;
    }
    if(argc<2||argc>3){std::cout<<"File Trace Studio - observational file I/O completion intervals\nUsage: FileTraceCLI.exe <PID> [seconds]\n       FileTraceCLI.exe --sample\nCapture needs an allocated-altitude, Microsoft-signed driver and administrator access.\n";return 2;}
    uint32_t pid=0,seconds=10;
    if(!Studio::ParsePid(argv[1],pid)||(argc==3&&(!Studio::ParsePid(argv[2],seconds)||seconds>3600))){std::cerr<<"PID must be positive; duration must be 1 to 3600 seconds.\n";return 2;}
    FileTrace::Client client;FileTrace::Batch batch;DWORD error=client.Connect();if(!error)error=client.Start(pid,batch);
    if(error){std::cerr<<FileTrace::Error(error)<<" ("<<error<<")\n";return 1;}
    Studio::Model model;model.Add(batch);ULONGLONG end=GetTickCount64()+seconds*1000ull;
    while(GetTickCount64()<end){error=client.Read(batch);if(error)break;model.Add(batch);if(batch.events.empty())Sleep(20);}
    FileTrace::Batch finalStatus;DWORD stop=client.Stop(finalStatus);client.Close();
    if(error||stop){DWORD code=error?error:stop;std::cerr<<FileTrace::Error(code)<<" ("<<code<<")\n";return 1;}
    std::cout<<"Received: "<<model.total.count<<"  errors: "<<model.total.errors<<"  dropped: "<<model.status.Dropped<<"  mean observed interval: "<<model.MeanMicroseconds(model.total)<<" us\n";
    std::cout<<"These intervals are not disk latency; bypass/paging/memory-mapped activity is outside coverage.\n";return 0;
}

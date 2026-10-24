#include "FileTrace.h"
#include <iostream>

int main() {
    FileTrace::Client trace;
    FileTrace::Batch batch;
    DWORD error=trace.Connect();
    if(!error)error=trace.Start(GetCurrentProcessId(),batch);
    if(error){std::cerr<<FileTrace::Error(error);return 1;}

    // Observe a unique temporary file in this process. Never overwrite a user file.
    wchar_t folder[MAX_PATH]{};
    DWORD length=GetTempPathW(MAX_PATH,folder);
    if(!length||length>=MAX_PATH)return 1;
    std::wstring path=std::wstring(folder)+L"FileTrace-example-"+
        std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".tmp";
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,nullptr);
    if(file==INVALID_HANDLE_VALUE){std::cerr<<FileTrace::Error(GetLastError());return 1;}
    DWORD written=0;
    bool writtenOk=WriteFile(file,"example",7,&written,nullptr)&&written==7;
    CloseHandle(file);
    if(!writtenOk){std::cerr<<"Temporary test write did not complete.";return 1;}

    do {
        error=trace.Read(batch);
        if(error){std::cerr<<FileTrace::Error(error);return 1;}
        for(const auto& event:batch.events)
            std::cout<<FileTrace::Operation(event.Operation)<<" "
                <<FileTrace::Microseconds(event,batch.status.Frequency)<<" us\n";
    }while(!batch.events.empty());
    trace.Stop(batch); // Closing the owning connection also stops capture.
}

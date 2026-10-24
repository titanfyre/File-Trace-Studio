#include "Model.h"
#include <commdlg.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <atomic>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

namespace {
constexpr COLORREF Background=RGB(13,19,32),Panel=RGB(23,33,51),Ink=RGB(239,244,255),Muted=RGB(163,179,204),Blue=RGB(39,104,241);
HWND window{},pidEdit{},startButton{},stopButton{},sampleButton{},exportButton{};HFONT titleFont{},bodyFont{},smallFont{};
Studio::Model model;std::mutex shared;std::wstring status=L"Ready - capture is off",detail=L"Select a process ID and start an explicit read-only capture. No file contents or paths are recorded.";
HANDLE stopEvent{};std::thread worker;bool active=false,synthetic=false;std::atomic_bool done=false;
std::wstring Number(double n){std::wostringstream out;out.imbue(std::locale::classic());out<<std::fixed<<std::setprecision(2)<<n;return out.str();}
std::wstring Widen(const std::string& value){if(value.empty())return {};int count=MultiByteToWideChar(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0);std::wstring result(count,L' ');MultiByteToWideChar(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),count);return result;}
void Text(HDC dc,int x,int y,int right,int bottom,const std::wstring& text,HFONT font,COLORREF color,UINT flags=DT_LEFT|DT_WORDBREAK){auto old=SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);RECT r{x,y,right,bottom};DrawTextW(dc,text.c_str(),-1,&r,flags);SelectObject(dc,old);}
void Draw(HDC dc,const RECT& r){
    Studio::Model copy;std::wstring message,explanation;{std::lock_guard<std::mutex> guard(shared);copy=model;message=status;explanation=detail;}
    HBRUSH brush=CreateSolidBrush(Background);FillRect(dc,&r,brush);DeleteObject(brush);
    Text(dc,30,20,r.right-30,66,L"File Trace Studio",titleFont,Ink);
    Text(dc,30,68,r.right-30,94,L"PID-selected file I/O diagnostics / OEM integration SDK",smallFont,Muted);
    Text(dc,30,111,125,138,L"Process ID",smallFont,Muted);
    RECT inputBox{125,105,250,139};brush=CreateSolidBrush(Panel);FillRect(dc,&inputBox,brush);DeleteObject(brush);
    wchar_t inputText[32]{};GetWindowTextW(pidEdit,inputText,32);Text(dc,134,111,244,135,*inputText?inputText:L"e.g. 4242",smallFont,Muted,DT_SINGLELINE);
    RECT card{30,174,r.right-30,275};brush=CreateSolidBrush(Panel);FillRect(dc,&card,brush);DeleteObject(brush);
    Text(dc,48,187,r.right-48,217,message,bodyFont,Ink);Text(dc,48,222,r.right-48,245,explanation,smallFont,Muted,DT_SINGLELINE|DT_END_ELLIPSIS);
    if(copy.hasLast){wchar_t code[24]{};swprintf_s(code,L"0x%08X",static_cast<uint32_t>(copy.last.Status));Text(dc,48,249,r.right-48,271,L"Last: "+Widen(FileTrace::Operation(copy.last.Operation))+L" / NTSTATUS "+code+L" / "+Number(FileTrace::Microseconds(copy.last,copy.status.Frequency))+L" us",smallFont,Muted);}
    int column=(r.right-60)/3;
    Text(dc,30,294,30+column,320,L"RECEIVED EVENTS",smallFont,Muted);Text(dc,30,320,30+column,358,std::to_wstring(copy.total.count),titleFont,Ink);
    Text(dc,30+column,294,30+2*column,320,L"ERRORS / DROPPED",smallFont,Muted);Text(dc,30+column,320,30+2*column,358,std::to_wstring(copy.total.errors)+L" / "+std::to_wstring(copy.status.Dropped),titleFont,Ink);
    Text(dc,30+2*column,294,r.right-30,320,L"MEAN OBSERVED INTERVAL",smallFont,Muted);Text(dc,30+2*column,320,r.right-30,358,Number(copy.MeanMicroseconds(copy.total))+L" us",titleFont,Ink);
    RECT chart{30,380,r.right-30,510};brush=CreateSolidBrush(Panel);FillRect(dc,&chart,brush);DeleteObject(brush);
    Text(dc,44,387,r.right-44,411,L"Recent completion intervals - microseconds",smallFont,Muted);
    if(copy.history.size()>1){double maximum=(std::max)(1.0,*std::max_element(copy.history.begin(),copy.history.end()));HPEN pen=CreatePen(PS_SOLID,2,Blue);auto old=SelectObject(dc,pen);size_t i=0;
        for(double value:copy.history){int x=chart.left+16+static_cast<int>(i*(chart.right-chart.left-32)/(copy.history.size()-1));int y=chart.bottom-14-static_cast<int>((chart.bottom-chart.top-52)*value/maximum);if(i++)LineTo(dc,x,y);else MoveToEx(dc,x,y,nullptr);}
        SelectObject(dc,old);DeleteObject(pen);
    }
    constexpr uint16_t codes[]={0,3,4,6,18};int y=531;
    Text(dc,35,y,300,y+24,L"OPERATION",smallFont,Muted);Text(dc,330,y,460,y+24,L"EVENTS",smallFont,Muted);Text(dc,510,y,650,y+24,L"ERRORS",smallFont,Muted);Text(dc,690,y,r.right-35,y+24,L"MEAN",smallFont,Muted);y+=29;
    for(size_t i=0;i<5;++i){const auto& entry=copy.operations[i];Text(dc,35,y,300,y+24,Widen(FileTrace::Operation(codes[i])),smallFont,Ink);Text(dc,330,y,460,y+24,std::to_wstring(entry.count),smallFont,Ink);Text(dc,510,y,650,y+24,std::to_wstring(entry.errors),smallFont,Ink);Text(dc,690,y,r.right-35,y+24,Number(copy.MeanMicroseconds(entry))+L" us",smallFont,Ink);y+=29;}
    Text(dc,30,725,r.right-30,780,L"Engineering preview: allocated altitude, Microsoft signing and runtime validation pending.\nIntervals are measured at this filter. Paging, memory-mapped and BypassIO activity is outside coverage.",smallFont,Muted);
}
void Stop(){if(worker.joinable()){SetEvent(stopEvent);worker.join();}active=false;EnableWindow(startButton,TRUE);EnableWindow(pidEdit,TRUE);EnableWindow(stopButton,FALSE);}
void Start(){
    if(active)return;wchar_t buffer[32]{};GetWindowTextW(pidEdit,buffer,32);uint32_t pid=0;
    if(!Studio::ParsePid(buffer,pid)){std::lock_guard<std::mutex> guard(shared);status=L"Enter a valid positive process ID";detail=L"Find the PID in Task Manager > Details. No capture has started.";InvalidateRect(window,nullptr,FALSE);return;}
    Stop();ResetEvent(stopEvent);done=false;synthetic=false;active=true;EnableWindow(startButton,FALSE);EnableWindow(pidEdit,FALSE);EnableWindow(stopButton,TRUE);
    {std::lock_guard<std::mutex> guard(shared);model.Clear();status=L"Connecting...";detail=L"Administrator access and a signed driver with allocated altitude are required.";}
    try{worker=std::thread([pid]{try{
        FileTrace::Client client;FileTrace::Batch batch;DWORD error=client.Connect();if(!error)error=client.Start(pid,batch);
        if(!error){std::lock_guard<std::mutex> guard(shared);model.Add(batch);status=L"Capturing PID "+std::to_wstring(pid);detail=L"Read-only completion intervals. Closing this window stops capture.";}
        unsigned burst=0;
        while(!error&&WaitForSingleObject(stopEvent,0)!=WAIT_OBJECT_0){
            error=client.Read(batch);if(error)break;{std::lock_guard<std::mutex> guard(shared);model.Add(batch);}
            if(batch.events.empty()){burst=0;WaitForSingleObject(stopEvent,20);}else if(++burst>=32){burst=0;WaitForSingleObject(stopEvent,1);}
        }
        if(!error){FileTrace::Batch ignored;error=client.Stop(ignored);}client.Close();
        {std::lock_guard<std::mutex> guard(shared);status=error?L"Capture not completed":L"Capture stopped";detail=error?Widen(FileTrace::Error(error))+L" ("+std::to_wstring(error)+L")":L"The driver is inactive. Received summary remains available for export.";}
        }catch(...){try{std::lock_guard<std::mutex> guard(shared);status=L"Capture stopped after a local resource error";detail=L"The owning connection was closed; no capture client remains active.";}catch(...){}}
        done=true;
    });}catch(...){active=false;done=true;std::lock_guard<std::mutex> guard(shared);status=L"Worker could not start";detail=L"No capture is running.";}
}
bool Preview(const std::wstring& path){
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token{};if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)return false;bool saved=false;
    {RECT r;GetClientRect(window,&r);Gdiplus::Bitmap bitmap(r.right,r.bottom,PixelFormat32bppARGB);Gdiplus::Graphics graphics(&bitmap);HDC dc=graphics.GetHDC();Draw(dc,r);
    for(HWND child:{pidEdit,startButton,stopButton,sampleButton,exportButton}){RECT area;GetWindowRect(child,&area);POINT p{area.left,area.top};ScreenToClient(window,&p);int old=SaveDC(dc);SetViewportOrgEx(dc,p.x,p.y,nullptr);SendMessageW(child,WM_PRINT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);RestoreDC(dc,old);}
    graphics.ReleaseHDC(dc);CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0,0,0xf8,0x1e,0xf3,0x2e}};saved=bitmap.Save(path.c_str(),&png,nullptr)==Gdiplus::Ok;}
    Gdiplus::GdiplusShutdown(token);return saved;
}
void Sample(){Stop();synthetic=true;{std::lock_guard<std::mutex> guard(shared);model.Clear();model.Add(FileTrace::Sample());status=L"OFFLINE SAMPLE - synthetic data";detail=L"Illustration only. No driver connection, capture or installation was made.";}InvalidateRect(window,nullptr,FALSE);}
void Export(){wchar_t path[32768]=L"File-Trace-report.json";OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=window;dialog.lpstrFilter=L"JSON report\0*.json\0";dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.lpstrDefExt=L"json";dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(!GetSaveFileNameW(&dialog))return;
    DWORD error;{std::lock_guard<std::mutex> guard(shared);error=Studio::Export(path,model,synthetic);status=error?L"Report was not saved":L"Report saved";detail=error?Widen(FileTrace::Error(error)):L"Local JSON summary. Existing files are preserved; choose a new name for another export.";}InvalidateRect(window,nullptr,FALSE);
}
LRESULT CALLBACK Proc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
    switch(message){case WM_CREATE:{
        titleFont=CreateFontW(30,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");bodyFont=CreateFontW(20,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");smallFont=CreateFontW(16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        pidEdit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,125,105,125,34,hwnd,reinterpret_cast<HMENU>(1),nullptr,nullptr);SendMessageW(pidEdit,EM_SETLIMITTEXT,10,0);
        startButton=CreateWindowW(L"BUTTON",L"Start capture",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW|WS_TABSTOP,266,105,148,34,hwnd,reinterpret_cast<HMENU>(2),nullptr,nullptr);
        stopButton=CreateWindowW(L"BUTTON",L"Stop",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW|WS_TABSTOP,425,105,100,34,hwnd,reinterpret_cast<HMENU>(3),nullptr,nullptr);
        sampleButton=CreateWindowW(L"BUTTON",L"Offline sample",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW|WS_TABSTOP,537,105,145,34,hwnd,reinterpret_cast<HMENU>(4),nullptr,nullptr);
        exportButton=CreateWindowW(L"BUTTON",L"Export JSON",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW|WS_TABSTOP,694,105,146,34,hwnd,reinterpret_cast<HMENU>(5),nullptr,nullptr);
        for(HWND c:{pidEdit,startButton,stopButton,sampleButton,exportButton})SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(smallFont),TRUE);
        EnableWindow(stopButton,FALSE);SetTimer(hwnd,1,100,nullptr);return 0;}
    case WM_COMMAND:if(HIWORD(w)==BN_CLICKED){switch(LOWORD(w)){case 2:Start();break;case 3:Stop();break;case 4:Sample();break;case 5:Export();break;}}return 0;
    case WM_TIMER:if(done.exchange(false))Stop();InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_SETCURSOR:if(GetDlgCtrlID(reinterpret_cast<HWND>(w))>=2&&GetDlgCtrlID(reinterpret_cast<HWND>(w))<=5){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}break;
    case WM_ERASEBKGND:return 1;
    case WM_DRAWITEM:{auto item=reinterpret_cast<DRAWITEMSTRUCT*>(l);bool enabled=(item->itemState&ODS_DISABLED)==0;HBRUSH b=CreateSolidBrush(enabled?Blue:Panel);FillRect(item->hDC,&item->rcItem,b);DeleteObject(b);wchar_t text[64]{};GetWindowTextW(item->hwndItem,text,64);Text(item->hDC,item->rcItem.left,item->rcItem.top,item->rcItem.right,item->rcItem.bottom,text,smallFont,enabled?Ink:Muted,DT_SINGLELINE|DT_CENTER|DT_VCENTER);if(item->itemState&ODS_FOCUS)DrawFocusRect(item->hDC,&item->rcItem);return TRUE;}
    case WM_PAINT:{PAINTSTRUCT paint;HDC dc=BeginPaint(hwnd,&paint);RECT r;GetClientRect(hwnd,&r);HDC memory=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);auto old=SelectObject(memory,bitmap);Draw(memory,r);BitBlt(dc,0,0,r.right,r.bottom,memory,0,0,SRCCOPY);SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);EndPaint(hwnd,&paint);return 0;}
    case WM_CLOSE:Stop();DestroyWindow(hwnd);return 0;
    case WM_DESTROY:KillTimer(hwnd,1);DeleteObject(titleFont);DeleteObject(bodyFont);DeleteObject(smallFont);PostQuitMessage(0);return 0;
    }return DefWindowProcW(hwnd,message,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);int argc=0;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);std::wstring preview;
    bool sample=false;for(int i=1;i<argc;++i){std::wstring arg=argv[i];if(arg==L"--sample")sample=true;else if(arg==L"--preview"&&i+1<argc){sample=true;preview=argv[++i];}else{if(argv)LocalFree(argv);return 2;}}if(argv)LocalFree(argv);
    stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!stopEvent)return 1;WNDCLASSW type{};type.hInstance=instance;type.lpszClassName=L"TitanFileTraceStudio";type.lpfnWndProc=Proc;type.hCursor=LoadCursorW(nullptr,IDC_ARROW);if(!RegisterClassW(&type)){CloseHandle(stopEvent);return 1;}
    RECT bounds{0,0,875,795};AdjustWindowRect(&bounds,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE);
    window=CreateWindowW(type.lpszClassName,L"File Trace Studio",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,instance,nullptr);
    if(!window){CloseHandle(stopEvent);return 1;}if(sample)Sample();if(!preview.empty()){bool ok=Preview(preview);DestroyWindow(window);CloseHandle(stopEvent);return ok?0:1;}
    ShowWindow(window,show);MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
    Stop();CloseHandle(stopEvent);return static_cast<int>(message.wParam);
}

#include "quest/assistant_bridge.hpp"
#include "quest/backend.hpp"
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
namespace quest {
AssistantBridge::~AssistantBridge(){stop();}
static std::wstring argQuote(const std::wstring& s){std::wstring out=L"\"";size_t n=0;for(wchar_t c:s){if(c==L'\\'){++n;continue;}out.append(c==L'"'?2*n+1:n,L'\\');out+=c;n=0;}out.append(n*2,L'\\');return out+L'"';}
bool AssistantBridge::start(const std::wstring& python,const std::wstring& script,const std::wstring& data,std::string& error){
    stop();error.clear();std::error_code ec;if(!std::filesystem::is_regular_file(python,ec)){error="Runtime IA absent. Exécutez runtime/setup.ps1 ou utilisez Installer / réparer.";return false;}
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE inRead{},inWrite{},outRead{},outWrite{};
    if(!CreatePipe(&inRead,&inWrite,&sa,0))return false;if(!CreatePipe(&outRead,&outWrite,&sa,0)){CloseHandle(inRead);CloseHandle(inWrite);return false;}
    SetHandleInformation(inWrite,HANDLE_FLAG_INHERIT,0);SetHandleInformation(outRead,HANDLE_FLAG_INHERIT,0);
    std::filesystem::create_directories(data,ec);auto log=std::filesystem::path(data)/L"runtime-errors.log";HANDLE stderrFile=CreateFileW(log.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=inRead;si.hStdOutput=outWrite;si.hStdError=stderrFile;
    PROCESS_INFORMATION pi{};auto cmd=argQuote(python)+L" -u "+argQuote(script)+L" --data-dir "+argQuote(data);
    BOOL ok=CreateProcessW(python.c_str(),cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&si,&pi);
    auto code=GetLastError();CloseHandle(inRead);CloseHandle(outWrite);if(stderrFile!=INVALID_HANDLE_VALUE)CloseHandle(stderrFile);
    if(!ok){CloseHandle(inWrite);CloseHandle(outRead);error="Démarrage du runtime impossible (Windows "+std::to_string(code)+").";return false;}
    job_=CreateJobObjectW(nullptr,nullptr);if(job_){JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;SetInformationJobObject(job_,JobObjectExtendedLimitInformation,&limits,sizeof(limits));AssignProcessToJobObject(job_,pi.hProcess);}
    ResumeThread(pi.hThread);CloseHandle(pi.hThread);process_=pi.hProcess;input_=inWrite;output_=outRead;stopping_=false;running_=true;
    reader_=std::thread([this]{std::string pending;char b[16384];while(!stopping_){DWORD available=0,n=0;if(PeekNamedPipe(output_,nullptr,0,nullptr,&available,nullptr)&&available){if(ReadFile(output_,b,std::min(available,DWORD(sizeof(b))),&n,nullptr)&&n){pending.append(b,n);size_t at=0;while((at=pending.find('\n'))!=std::string::npos){auto line=pending.substr(0,at);pending.erase(0,at+1);auto value=Json::parse(line,nullptr,false);if(!value.is_discarded()){std::lock_guard<std::mutex> lock(mutex_);if(eventBytes_<8*1024*1024){eventBytes_+=line.size();events_.push_back(std::move(value));}}}if(pending.size()>1024*1024)pending.clear();}}
        else if(WaitForSingleObject(process_,10)==WAIT_OBJECT_0)break;else Sleep(5);
    }running_=false;});return true;
}
bool AssistantBridge::send(const Json& value){std::lock_guard<std::mutex> lock(writeMutex_);if(!running_||!input_)return false;auto data=value.dump(-1,' ',false,Json::error_handler_t::replace)+"\n";size_t offset=0;while(offset<data.size()){DWORD n=0;if(!WriteFile(input_,data.data()+offset,DWORD(data.size()-offset),&n,nullptr)||!n)return false;offset+=n;}return true;}
std::vector<Json> AssistantBridge::poll(){std::lock_guard<std::mutex> lock(mutex_);std::vector<Json> result;while(!events_.empty()){result.push_back(std::move(events_.front()));events_.pop_front();}eventBytes_=0;return result;}
void AssistantBridge::stop(){if(process_){send({{"type","shutdown"}});if(WaitForSingleObject(process_,2500)!=WAIT_OBJECT_0){if(job_)TerminateJobObject(job_,1);else TerminateProcess(process_,1);}}stopping_=true;if(reader_.joinable())reader_.join();if(input_)CloseHandle(input_);if(output_)CloseHandle(output_);if(process_)CloseHandle(process_);if(job_)CloseHandle(job_);input_=output_=process_=job_=nullptr;running_=false;std::lock_guard<std::mutex> lock(mutex_);events_.clear();eventBytes_=0;}
}

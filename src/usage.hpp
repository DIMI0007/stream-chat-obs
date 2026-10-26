#pragma once
#include <atomic>
#include <cstdint>
#include <windows.h>

namespace sc {
inline std::atomic<std::uint64_t> measuredCpuNs{0};
inline bool threadCpuNs(std::uint64_t& value){
    FILETIME created,exited,kernel,user;
    if(!GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user))return false;
    ULARGE_INTEGER k,u;k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;
    u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;
    value=(k.QuadPart+u.QuadPart)*100;return true;
}
class CpuScope {
public:
    CpuScope():outer_(depth_++==0){valid_=outer_&&threadCpuNs(start_);}
    ~CpuScope(){--depth_;std::uint64_t end=0;if(valid_&&threadCpuNs(end)&&end>=start_)measuredCpuNs.fetch_add(end-start_,std::memory_order_relaxed);}
    CpuScope(const CpuScope&)=delete;CpuScope& operator=(const CpuScope&)=delete;
private:
    inline static thread_local unsigned depth_=0;
    std::uint64_t start_=0;bool outer_=false,valid_=false;
};
}

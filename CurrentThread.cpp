#include "CurrentThread.h"

namespace CurrentThread {
    __thread int t_cachedTid = 0;

    void cacheTid(){
        if(!t_cachedTid){
            t_cachedTid = static_cast<int>(::syscall(SYS_gettid));
        }
    }
}
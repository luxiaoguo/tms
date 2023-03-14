#ifndef LOG_H
#define LOG_H

#include "Utils.h"

//  __FILE__ 获取源文件的相对路径和名字
//  __LINE__ 获取该行代码在文件中的行号
//  __func__ 或 __FUNCTION__ 获取函数名

enum LOG_LEV
{
    log_error = 1,
    log_info = 2,
    log_debug = 3
    
};
extern int G_LOG_LEV;

#define LOGI(format, ...) if(G_LOG_LEV >= log_info) fprintf(stderr,"[\033[0;32mINFO\033[0m] %s [%s:%d %s()] " format "\n", getCurFormatTime().data(),__FILE__,__LINE__,__func__ ,##__VA_ARGS__)
#define LOGD(format, ...) if(G_LOG_LEV >= log_debug) fprintf(stderr,"[\033[0;33mDEBUG\033[0m] %s [%s:%d %s()] " format "\n", getCurFormatTime().data(),__FILE__,__LINE__,__func__ ,##__VA_ARGS__)
#define LOGE(format, ...) if(G_LOG_LEV >= log_error) fprintf(stderr,"[\033[0;31mERROR\033[0m] %s [%s:%d %s()] " format "\n",getCurFormatTime().data(),__FILE__,__LINE__,__func__ ,##__VA_ARGS__)

#endif //
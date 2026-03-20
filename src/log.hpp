/*
This file is part of Snappy Driver Installer.

Snappy Driver Installer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License or (at your option) any later version.

Snappy Driver Installer is distributed in the hope that it will be useful
but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
Snappy Driver Installer.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef GUICON_H
#define GUICON_H

#include <cstdio>

// Timer
enum timer:uint8_t
{
    time_total,
    time_startup,
    time_indices,
    time_devicescan,
    time_chkupdate,
    time_indexsave,
    time_indexprint,
    time_sysinfo,
    time_matcher,
    time_test,
    time_NUM,
};

class timers_t
{
    long long timers[time_NUM];

public:
    timers_t();
    void start(int a);
    void stop(int a);
    void reset(int a){timers[a]=0;}
    void print();
    void stoponce(int a,int b);
    long long get(int a){return timers[a];}
};
extern timers_t Timers;

// Logging
enum LOG_VERBOSE
{
    LOG_VERBOSE_ARGS      = 0x0001,
    LOG_VERBOSE_SYSINFO   = 0x0002,
    LOG_VERBOSE_DEVICES   = 0x0004,
    LOG_VERBOSE_MATCHER   = 0x0008,
    LOG_VERBOSE_MANAGER   = 0x0010,
    LOG_VERBOSE_DRP       = 0x0020,
    LOG_VERBOSE_TIMES     = 0x0040,
    LOG_VERBOSE_LOG_ERR   = 0x0080,
    LOG_VERBOSE_LOG_CON   = 0x0100,
    LOG_VERBOSE_LAGCOUNTER= 0x0200,
    LOG_VERBOSE_DEVSYNC   = 0x0400,
    LOG_VERBOSE_BATCH     = 0x0800,
    LOG_VERBOSE_DEBUG     = 0x1000,
    LOG_VERBOSE_TORRENT   = 0x2000,
};

#define print_index print_nul

class Log_t
{
private:
    wchar_t timestamp[MAX_COMPUTERNAME_LENGTH + 24];
    FILE *logfile=nullptr;
    int error_count=0;
    int log_console=0;
    int log_verbose=
        LOG_VERBOSE_ARGS|
        LOG_VERBOSE_SYSINFO|
        LOG_VERBOSE_DEVICES|
        LOG_VERBOSE_MATCHER|
        LOG_VERBOSE_MANAGER|
        LOG_VERBOSE_DRP|
        LOG_VERBOSE_TIMES|
        LOG_VERBOSE_LOG_ERR|
        LOG_VERBOSE_LOG_CON|
        //LOG_VERBOSE_DEBUG|
        //LOG_VERBOSE_DEVSYNC|
        0;

    void output_con(const char *buffer);
    void output_file(const char *buffer);
    void output_err(const char *buffer);
    void output_warn(const char *buffer);
    void output_debug(const char *buffer);

public:
    template<typename... Args>
    void console(_Printf_format_string_ char const *format,Args... args)
    {
        if((log_verbose&(LOG_VERBOSE_LOG_CON|LOG_VERBOSE_DEBUG))==0)return;
        char buffer[1024*16];
        snprintf(buffer,sizeof(buffer),format,args...);
        buffer[sizeof(buffer)-1]=0;
        output_con(buffer);
    }

    template<typename... Args>
    void file(_Printf_format_string_ char const *format,Args... args)
    {
        if(!logfile)return;
        char buffer[1024*16];
        snprintf(buffer,sizeof(buffer),format,args...);
        buffer[sizeof(buffer)-1]=0;
        output_file(buffer);
    }

    template<typename... Args>
    void error(_Printf_format_string_ char const *format,Args... args)
    {
        if((log_verbose&(LOG_VERBOSE_LOG_ERR|LOG_VERBOSE_DEBUG))==0)return;
        char buffer[1024*16];
        snprintf(buffer,sizeof(buffer),format,args...);
        buffer[sizeof(buffer)-1]=0;
        output_err(buffer);
    }

    template<typename... Args>
    void warning(_Printf_format_string_ char const *format,Args... args)
    {
        if((log_verbose&(LOG_VERBOSE_LOG_ERR|LOG_VERBOSE_DEBUG))==0)return;
        char buffer[1024*16];
        snprintf(buffer,sizeof(buffer),format,args...);
        buffer[sizeof(buffer)-1]=0;
        output_warn(buffer);
    }

    template<typename... Args>
    void debug(_Printf_format_string_ char const *format,Args... args)
    {
        if((log_verbose&LOG_VERBOSE_DEBUG)==0)return;
        char buffer[1024*16];
        snprintf(buffer,sizeof(buffer),format,args...);
        buffer[sizeof(buffer)-1]=0;
        output_debug(buffer);
    }

    template<typename... Args>
    static void print_nul(_Printf_format_string_ char const */*format*/,Args... /*args*/){}

    void system_error(int r,const wchar_t *s);

    void gen_timestamp();
    void start(wchar_t *log_dir);
    void save();
    void stop();

    bool isAllowed(int a){return (log_verbose&a)==a;}
    bool isHidden(int a){return !(log_verbose&a);}
    void set_verbose(int a){log_verbose=a;}
    int get_verbose(){return log_verbose;}
    void set_mode(int a){log_console=a;}
    int getErrorCount(){return error_count;}
    wchar_t *getTimestamp(){return timestamp;}

    friend class Settings_t;
};
extern Log_t Log;

// Error handling
const wchar_t *errno_str();
//void start_exception_handlers();
//void SignalHandler(int signum);

// Virus detection
//void viruscheck(const wchar_t *szFile,int action,int lParam);

#endif

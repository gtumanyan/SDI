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

#include "com_header.h"
#include "common.h"
#include "logging.h"
#include "settings.h"
#include "system.h"

#include <windows.h>
#include <ctime>
#ifdef _MSC_VER
#include <cerrno>
#include <clocale>
#endif

// Depend on Win32API
#include "main.h"

//{ Global variables
Log_t Log;
Timers_t Timers;
//}

//{ Logging

#ifdef _MSC_VER
Timers_t::Timers_t()
{
	timers[0]=timers[1]=timers[2]=timers[3]=timers[4]=
	timers[5]=timers[6]=timers[7]=timers[8]=timers[9]=0;
}
#else
Timers_t::Timers_t():
    timers{0,0,0,0,0,0,0,0,0,0}
{
}
#endif

void Timers_t::start(int a)
{
    timers[a]=System.GetTickCountWr();
}
void Timers_t::stop(int a)
{
    if(timers[a])timers[a]=System.GetTickCountWr()-timers[a];
}
void Timers_t::stoponce(int a,int b)
{
    if(!timers[a])timers[a]=System.GetTickCountWr()-timers[b];
}

void Timers_t::print()
{
    if(Log.isHidden(LOG_VERBOSE_TIMES))return;
    Log.print_con("Times\n");
    Log.print_con("  devicescan: %7ld (%d errors)\n",timers[time_devicescan],Log.getErrorCount());
    Log.print_con("  indexes:    %7ld\n",timers[time_indexes]);
    Log.print_con("  sysinfo:    %7ld\n",timers[time_sysinfo]);
    Log.print_con("  matcher:    %7ld\n",timers[time_matcher]);
    Log.print_con("  chkupdate:  %7ld\n",timers[time_chkupdate]);
    Log.print_con("  startup:    %7ld (%ld)\n",timers[time_startup],timers[time_startup]-timers[time_devicescan]-timers[time_indexes]-timers[time_matcher]-timers[time_sysinfo]);
    Log.print_con("  indexsave:  %7ld\n",timers[time_indexsave]);
    Log.print_con("  indexprint: %7ld\n",timers[time_indexprint]);
    Log.print_con("  total:      %7ld\n",System.GetTickCountWr()-timers[time_total]);
    Log.print_con("  test:       %7ld\n",timers[time_test]);
}

void Log_t::gen_timestamp()
{
    wchar_t pcname[BUFLEN];
    time_t rawtime;
    struct tm *ti;
    DWORD sz=BUFLEN;

    GetComputerName(pcname,&sz);
    time(&rawtime);
    ti=localtime(&rawtime);
    if(Settings.flags&FLAG_NOSTAMP)
        *timestamp=0;
    else
        wsprintf(timestamp,L"%4d_%02d_%02d__%02d_%02d_%02d__%s_",
             1900+ti->tm_year,ti->tm_mon+1,ti->tm_mday,
             ti->tm_hour,ti->tm_min,ti->tm_sec,pcname);
}

void Log_t::start(wchar_t *logdir)
{
    WStringShort filename;

    if(Settings.flags&FLAG_NOLOGFILE)return;
    setlocale(LC_ALL,"");
    //system("chcp 1251");

    gen_timestamp();

    filename.sprintf(L"%s\\%slog.txt",logdir,timestamp);
    if(!(System.canWriteDirectory(logdir)&&System.canWriteFile(filename.Get(),L"wt")))
    {
        Log.print_err("ERROR in log_start(): Write-protected,'%S'\n",filename.Get());
        GetEnvironmentVariable(L"TEMP",logdir,BUFLEN);
        wcscat(logdir,L"\\SDI_logs");
        filename.sprintf(L"%s\\%slog.txt",logdir,timestamp);
    }

    logfile=_wfopen(filename.Get(),L"wt");
    if(!logfile)
    {
        Log.print_err("ERROR in log_start(): Write-protected,'%S'\n",filename.Get());
        GetEnvironmentVariable(L"TEMP",logdir,BUFLEN);
        wcscat(logdir,L"\\SDI_logs");
        filename.sprintf(L"%s\\%slog.txt",logdir,timestamp);
        mkdir_r(logdir);
        logfile=_wfopen(filename.Get(),L"wb");
    }
    if((log_verbose&LOG_VERBOSE_BATCH)==0)
        Log.print_file("{start logging\n%s\n\n", GIT_REV_STR);
}

void Log_t::save()
{
    if(!logfile)return;
    fflush(logfile);
}

void Log_t::stop()
{
    if(!logfile)return;
    if((log_verbose&LOG_VERBOSE_BATCH)==0)
        Log.print_file("}stop logging");
    fclose(logfile);
}

void Log_t::print_file(char const *format,...)
{
    char buffer[1024*16];

    if(!logfile)return;
    va_list args;
    va_start(args,format);
    vsprintf(buffer,format,args);
    fputs(buffer,logfile);
    if(log_console)fputs(buffer,stdout);
    va_end(args);
}

void Log_t::print_err(char const *format,...)
{
    char buffer[1024*16];

    if((log_verbose&(LOG_VERBOSE_LOG_ERR|LOG_VERBOSE_DEBUG))==0)return;
    va_list args;
    va_start(args,format);
    vsprintf(buffer,format,args);
    if(logfile)fputs(buffer,logfile);
    fputs(buffer,stdout);
    va_end(args);
}

void Log_t::print_con(char const *format,...)
{
    char buffer[1024*16];

    if((log_verbose&(LOG_VERBOSE_LOG_CON|LOG_VERBOSE_DEBUG))==0)return;
    va_list args;
    va_start(args,format);
    wvsprintfA(buffer,format,args);
    if(logfile)fputs(buffer,logfile);
    fputs(buffer,stdout);
    va_end(args);
}

void Log_t::print_nul(char const *format,...)
{
    UNREFERENCED_PARAMETER(format);
}

void Log_t::print_debug(char const *format,...)
{
    char buffer[1024*16];

    if((log_verbose&LOG_VERBOSE_DEBUG)==0)return;
    va_list args;
    va_start(args,format);
    wvsprintfA(buffer,format,args);
    if(logfile)fputs(buffer,logfile);
    fputs(buffer,stdout);
    va_end(args);
}
//}

//{ Error handling
const wchar_t *errno_str()
{
    switch(errno)
    {
        case EPERM:               return L"Operation not permitted";
        case ENOENT:              return L"No such file or directory";
        case ESRCH:               return L"No such process";
        case EINTR:               return L"Interrupted function";
        case EIO:                 return L"I/O error";
        case ENXIO:               return L"No such device or address";
        case E2BIG:               return L"Argument list too long";
        case ENOEXEC:             return L"Exec format error";
        case EBADF:               return L"Bad file number";
        case ECHILD:              return L"No spawned processes";
        case EAGAIN:              return L"No more processes or not enough memory or maximum nesting level reached";
        case ENOMEM:              return L"Not enough memory";
        case EACCES:              return L"Permission denied";
        case EFAULT:              return L"Bad address";
        case EBUSY:               return L"Device or resource busy";
        case EEXIST:              return L"File exists";
        case EXDEV:               return L"Cross-device link";
        case ENODEV:              return L"No such device";
        case ENOTDIR:             return L"Not a directory";
        case EISDIR:              return L"Is a directory";
        case EINVAL:              return L"Invalid argument";
        case ENFILE:              return L"Too many files open in system";
        case EMFILE:              return L"Too many open files";
        case ENOTTY:              return L"Inappropriate I/O control operation";
        case EFBIG:               return L"File too large";
        case ENOSPC:              return L"No space left on device";
        case ESPIPE:              return L"Invalid seek";
        case EROFS:               return L"Read-only file system";
        case EMLINK:              return L"Too many links";
        case EPIPE:               return L"Broken pipe";
        case EDOM:                return L"Math argument";
        case ERANGE:              return L"Result too large";
        case EDEADLK:             return L"Resource deadlock would occur";
        case ENAMETOOLONG:        return L"Filename too long";
        case ENOLCK:              return L"No locks available";
        case ENOSYS:              return L"Function not supported";
        case ENOTEMPTY:           return L"Directory not empty";
        case EILSEQ:              return L"Illegal byte sequence";
        default: return L"Unknown error";
    }
}

void Log_t::print_syserr(int r,const wchar_t *s)
{
    WString buf;
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,r,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),buf.GetV(),static_cast<DWORD>(buf.Length()),nullptr);
    Log.print_err("ERROR with %S:[%x]'%S'\n",s,r,buf.Get());
    error_count++;
}

static void myterminate()
{
    WStringShort buf;

    std::exception_ptr p;
    p=std::current_exception();

    try
    {
        std::rethrow_exception (p);
    }
    catch(const std::exception& e)
    {
        buf.sprintf(L"Exception: %S\n",e.what());
    }
    catch(int i)
    {
        buf.sprintf(L"Exception: %d\n",i);
    }
    catch(char const*str)
    {
        buf.sprintf(L"Exception: %S\n",str);
    }
    catch(wchar_t const*str)
    {
        buf.sprintf(L"Exception: %s\n",str);
    }
    catch(...)
    {
        buf.sprintf(L"Exception: unknown");
    }
    Log.print_err("ERROR: %S\n",buf.Get());
    Log.save();
    Log.stop();
    buf.append(L"\n\nThe program will self terminate now.");
    MessageBox(MainWindow.hMain,buf.Get(),L"Exception",MB_ICONERROR);

    abort();
}

static void myunexpected()
{
    Log.print_err("ERROR: myunexpected()\n");
    myterminate();
}

void start_exception_handlers()        //In the current Microsoft implementation of C++ exception handling, 
{                                      // unexpected calls terminate by default and is never called by the exception-handling run-time library. 
    std::set_unexpected(myunexpected); //  There is no particular advantage to calling unexpected rather than term`inate.
    std::set_terminate(myterminate);
}

void SignalHandler(int signum)
{
    Log.print_err("!!! Crashed %d!!!\n",signum);
    Log.save();
    Log.stop();
}

#undef new
void* operator new(size_t size, const char* file, int line)
{
    try
    {
        //Log.print_con("File '%s',Line %d,Size %d\n",file,line,size);
        return new char[size];
    }
    catch(...)
    {
        Log.print_con("File '%s',Line %d,Size %d\n",file,line,size);
        throw;
    }
}
void* operator new[](size_t size, const char* file, int line)
{
    try
    {
        //Log.print_con("File '%s',Line %d,Size %d\n",file,line,size);
        return new char[size];
    }
    catch(...)
    {
        Log.print_con("File '%s',Line %d,Size %d\n",file,line,size);
        throw;
    }
}
//#define new DEBUG_NEW

//}

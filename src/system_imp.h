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

//{ Event
class EventImp:public Event
{
    EventImp(const EventImp&)=delete;
	EventImp &operator = (const EventImp&) = delete;

    HANDLE h;

public:
    EventImp(bool manual):
        h(CreateEvent(nullptr,manual?1:0,0,nullptr))
    {
    }
    ~EventImp()
    {
        CloseHandle(h);
    }
    void wait()
    {
        WaitForSingleObject(h,INFINITE);
    }
    bool isRaised()
    {
        return WaitForSingleObject(h,0)==WAIT_OBJECT_0;
    }
    void raise()
    {
        SetEvent(h);
    }
    void reset()
    {
        ResetEvent(h);
    }
};
Event *CreateEventWr(bool manual)
{
    return new EventImp{manual};
}

//{ Thread
class ThreadImp:public ThreadAbs
{
    HANDLE h=nullptr;

public:
    void start(threadCallback callback,void *arg)
    {
        h=(HANDLE)_beginthreadex(nullptr,0,callback,arg,0,nullptr);
    }
    void join()
    {
        if(h)WaitForSingleObject(h,INFINITE);
    }
    ~ThreadImp()
    {
        if(h)
        {
            if(!CloseHandle(h))
                Log.print_err("ERROR in ThreadImpS(): failed CloseHandle\n");
        }
    }
};
ThreadAbs *CreateThread()
{
    return new ThreadImp;
}
//}

//{
class WindowWr
{
public:
    HWND handle;
};
//}

//{
class RectWr:public RECT
{
};
//}

//{ Filemon
struct FilemonDataPOD
{
	OVERLAPPED ol;
	HANDLE     hDir;
	BYTE       buffer[32*1024];
	LPARAM     lParam;
	DWORD      notifyFilter;
	BOOL       fStop;
	wchar_t    dir[BUFLEN];
	int        subdirs;
	FileChangeCallback callback;
};

class FilemonImp:public Filemon
{
    FilemonDataPOD data;

private:
    static void CALLBACK monitor_callback(DWORD dwErrorCode,DWORD dwNumberOfBytesTransfered,LPOVERLAPPED lpOverlapped);
    static int refresh(FilemonDataPOD &data);

public:
    FilemonImp(const wchar_t *szDirectory,int subdirs,FileChangeCallback callback);
    ~FilemonImp();
};
Filemon *CreateFilemon(const wchar_t *szDirectory,int subdirs,FileChangeCallback callback)
{
    return new FilemonImp(szDirectory,subdirs,callback);
}
//}

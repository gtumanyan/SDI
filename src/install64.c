/*
This file is part of Snappy Driver Installer Origin.

Snappy Driver Installer Origin is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License or (at your option) any later version.

Snappy Driver Installer Origin is distributed in the hope that it will be useful
but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
Snappy Driver Installer Origin.  If not, see <http://www.gnu.org/licenses/>.
*/

#if defined(UNICODE) && !defined(_UNICODE)
	#define _UNICODE
#elif defined(_UNICODE) && !defined(UNICODE)
	#define UNICODE
#endif

#include <windows.h>
#include <newdev.h>
#include <stdio.h>

static void print_error(int r,const WCHAR *s)
{
    WCHAR buf[4096];
    buf[0]=0;
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,r,0,(LPWSTR)&buf,4000,NULL);
    wprintf(L"ERROR with %ws:[%d]'%s'\n",s,r,buf);
}

int WINAPI WinMain(HINSTANCE hThisInstance,HINSTANCE hPrevInstance,LPSTR lpszArgument,int nCmdShow)
{
	UNREFERENCED_PARAMETER(hThisInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpszArgument);
	UNREFERENCED_PARAMETER(nCmdShow);

	WCHAR **argv;
    int argc;
    int ret=0,needreboot=0,lr;

	wprintf(L"Start\n");
	wprintf(L"Command: '%s'\n",GetCommandLineW());
    argv=CommandLineToArgvW(GetCommandLineW(),&argc);

	if(argc==3)
	{
		wprintf(L"Install64.exe '%s' '%s'\n",argv[1],argv[2]);
		ret=UpdateDriverForPlugAndPlayDevices(FindWindow(L"classSDIMain",0),argv[1],argv[2],INSTALLFLAG_FORCE,&needreboot);
	}
	else
		printf("argc=%d\n",argc);
	lr=GetLastError();
	wprintf(L"Finished %d,%d,%d(%X)\n",ret,needreboot,lr,lr);
	if(!ret)
	{
		ret=lr;
		print_error(lr,L"");
	}

	LocalFree(argv);
	if(!ret)return lr;
	return ret+(needreboot?0x80000000:0);
}

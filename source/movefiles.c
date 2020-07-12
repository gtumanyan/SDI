#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#pragma comment(lib, "User32.lib")

int error=0;

void movefile(char *file1,char *file2)
{
    FILE *f1,*f2;
    char c1,c2;

    f1=fopen(file1,"rb");
    f2=fopen(file2,"rb");
    if(!f2)
	{
		fclose(f1);
		return;
	}

    while(!feof(f1)&&!feof(f2))
    {
        c1=fgetc(f1);
        c2=fgetc(f2);
        if(c1!=c2)
        {
            printf("Conflict in %s AND %s\n",file1,file2);
            error=1;
            break;
        }
    }
    if(feof(f1)!=feof(f2))
    {
        printf("Conflict in %s AND %s\n",file1,file2);
        error=1;
    }

    fclose(f1);
    fclose(f2);
}

int movefolder(char *dir1,char *dir2,char *subdir)
{
    HANDLE hFind;
    WIN32_FIND_DATA ffd;
    TCHAR szDir[MAX_PATH];
    TCHAR newsubdir[MAX_PATH];
    DWORD dwError=0;

    sprintf(szDir,"%s%s*",dir1,subdir);

    hFind=FindFirstFile(szDir,&ffd);
    if(INVALID_HANDLE_VALUE==hFind)
    {
        printf("Error 1(%s)\n",szDir);
        return dwError;
    }

    do
    {
        if(ffd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY&&*ffd.cFileName=='.');else
        if(ffd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
        {
            sprintf(newsubdir,"%s%s\\",subdir,ffd.cFileName);
            movefolder(dir1,dir2,newsubdir);

        }
        else
        {
            char file1[MAX_PATH];
            char file2[MAX_PATH];

            sprintf(file1,"%s%s%s",dir1,subdir,ffd.cFileName);
            sprintf(file2,"%s%s%s",dir2,subdir,ffd.cFileName);
            //printf("%s%\n%s\n\n",file1,file2);
            movefile(file1,file2);
        }
    }
    while (FindNextFile(hFind, &ffd) != 0);

    dwError = GetLastError();
    if (dwError != ERROR_NO_MORE_FILES)
    {
        printf("Error 2\n");
    }

    FindClose(hFind);
    return dwError;
}

int main(int argc,TCHAR *argv[])
{
    if(strcmpi(argv[1],argv[2])==0)return 0;

    movefolder(argv[1],argv[2],"\\");

    if(!error)
    {
        char buf[MAX_PATH];
        sprintf(buf,"xcopy /E /Y /I %s %s",argv[1],argv[2]);
        printf("%s\n",buf);
        sprintf(buf,"xcopy /E /Y /I %s %s&rd /S /Q %s",argv[1],argv[2],argv[1]);
        system(buf);

        //sprintf(buf,"rd /S /Q %s",argv[1]);
        //system(buf);
    }
    return 0;
}

#include <windows.h>

static void
foo()
{
        int *f=NULL;
        *f = 0;
}

static void
bar()
{
        foo();
}

int
main()
{
#ifdef _WIN64
        LoadLibrary("backtrace64.dll");
#else
        LoadLibrary("backtrace.dll");
#endif
        bar();

        return 0;
}

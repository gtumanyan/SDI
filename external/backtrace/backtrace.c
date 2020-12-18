/*
        Copyright (c) 2010 ,
                Cloud Wu . All rights reserved.

                http://www.codingnow.com

        Use, modification and distribution are subject to the "New BSD License"
        as listed at <url: http://www.opensource.org/licenses/bsd-license.php >.

   filename: backtrace.c

   compiler: gcc 3.4.5 (mingw-win32)

   build command: gcc -O2 -shared -Wall -o backtrace.dll backtrace.c -lbfd -liberty -limagehlp

   how to use: Call LoadLibraryA("backtrace.dll"); at beginning of your program .

  */


#define PACKAGE "your-program-name"
#define PACKAGE_VERSION "1.2.3"
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <time.h>
#include <imagehlp.h>
#include <bfd.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_MAX (16*1024)

struct bfd_ctx {
        bfd * handle;
        asymbol ** symbol;
};

struct bfd_set {
        char * name;
        struct bfd_ctx * bc;
        struct bfd_set *next;
};

struct find_info {
        asymbol **symbol;
        bfd_vma counter;
        const char *file;
        const char *func;
        unsigned line;
};

struct output_buffer {
        char * buf;
        size_t sz;
        size_t ptr;
};

static void
output_init(struct output_buffer *ob, char * buf, size_t sz)
{
        ob->buf = buf;
        ob->sz = sz;
        ob->ptr = 0;
        ob->buf[0] = '\0';
}

static void
output_print(struct output_buffer *ob, const char * format, ...)
{
        if (ob->sz == ob->ptr)
                return;
        ob->buf[ob->ptr] = '\0';
        va_list ap;
        va_start(ap,format);
        vsnprintf(ob->buf + ob->ptr , ob->sz - ob->ptr , format, ap);
        va_end(ap);

        ob->ptr = strlen(ob->buf + ob->ptr) + ob->ptr;
}

static void
lookup_section(bfd *abfd, asection *sec, void *opaque_data)
{
        struct find_info *data = opaque_data;

        if (data->func)
                return;

        if (!(bfd_get_section_flags(abfd, sec) & SEC_ALLOC))
                return;

        bfd_vma vma = bfd_get_section_vma(abfd, sec);
        if (data->counter < vma || vma + bfd_get_section_size(sec) <= data->counter)
                return;

        bfd_find_nearest_line(abfd, sec, data->symbol, data->counter - vma, &(data->file), &(data->func), &(data->line));
}

static void
find(struct bfd_ctx * b, DWORD offset, const char **file, const char **func, unsigned *line)
{
        struct find_info data;
        data.func = NULL;
        data.symbol = b->symbol;
        data.counter = offset;
        data.file = NULL;
        data.func = NULL;
        data.line = 0;

        bfd_map_over_sections(b->handle, &lookup_section, &data);
        if (file) {
                *file = data.file;
        }
        if (func) {
                *func = data.func;
        }
        if (line) {
                *line = data.line;
        }
}

static int
init_bfd_ctx(struct bfd_ctx *bc, const char * procname, struct output_buffer *ob)
{
        bc->handle = NULL;
        bc->symbol = NULL;

        bfd *b = bfd_openr(procname, 0);
        if (!b) {
                output_print(ob,"Failed to open bfd from (%s)\n" , procname);
                return 1;
        }

        int r1 = bfd_check_format(b, bfd_object);
        int r2 = bfd_check_format_matches(b, bfd_object, NULL);
        int r3 = bfd_get_file_flags(b) & HAS_SYMS;

        if (!(r1 && r2 && r3)) {
                bfd_close(b);
                output_print(ob,"Failed to init bfd from (%s)\n", procname);
                return 1;
        }

        void *symbol_table;

        unsigned dummy = 0;
        if (bfd_read_minisymbols(b, FALSE, &symbol_table, &dummy) == 0) {
                if (bfd_read_minisymbols(b, TRUE, &symbol_table, &dummy) < 0) {
                        free(symbol_table);
                        bfd_close(b);
                        output_print(ob,"Failed to read symbols from (%s)\n", procname);
                        return 1;
                }
        }

        bc->handle = b;
        bc->symbol = symbol_table;

        return 0;
}

static void
close_bfd_ctx(struct bfd_ctx *bc)
{
        if (bc) {
                if (bc->symbol) {
                        free(bc->symbol);
                }
                if (bc->handle) {
                        bfd_close(bc->handle);
                }
        }
}

static struct bfd_ctx *
get_bc(struct output_buffer *ob , struct bfd_set *set , const char *procname)
{
        while(set->name) {
                if (strcmp(set->name , procname) == 0) {
                        return set->bc;
                }
                set = set->next;
        }
        struct bfd_ctx bc;
        if (init_bfd_ctx(&bc, procname , ob)) {
                return NULL;
        }
        set->next = calloc(1, sizeof(*set));
        set->bc = malloc(sizeof(struct bfd_ctx));
        memcpy(set->bc, &bc, sizeof(bc));
        set->name = strdup(procname);

        return set->bc;
}

static void
release_set(struct bfd_set *set)
{
        while(set) {
                struct bfd_set * temp = set->next;
                free(set->name);
                close_bfd_ctx(set->bc);
                free(set);
                set = temp;
        }
}

static void
_backtrace(struct output_buffer *ob, struct bfd_set *set, int depth , LPCONTEXT context)
{
        char procname[MAX_PATH];
        GetModuleFileNameA(NULL, procname, sizeof procname);

        struct bfd_ctx *bc = NULL;

        STACKFRAME frame;
        memset(&frame,0,sizeof(frame));

#ifdef _WIN64
        frame.AddrPC.Offset = context->Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context->Rsp;
        frame.AddrStack.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context->Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
#else
        frame.AddrPC.Offset = context->Eip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context->Esp;
        frame.AddrStack.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context->Ebp;
        frame.AddrFrame.Mode = AddrModeFlat;
#endif
        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();

        char symbol_buffer[sizeof(IMAGEHLP_SYMBOL) + 255];
        char module_name_raw[MAX_PATH];

#ifdef _WIN64
        while(StackWalk64(IMAGE_FILE_MACHINE_AMD64,
#else
        while(StackWalk(IMAGE_FILE_MACHINE_I386,
#endif
                process,
                thread,
                &frame,
                context,
                0,
                SymFunctionTableAccess,
                SymGetModuleBase, 0)) {

                --depth;
                if (depth < 0)
                        break;

                IMAGEHLP_SYMBOL *symbol = (IMAGEHLP_SYMBOL *)symbol_buffer;
                symbol->SizeOfStruct = (sizeof *symbol) + 255;
                symbol->MaxNameLength = 254;

#ifdef _WIN64
                DWORD64 module_base = SymGetModuleBase64(process, frame.AddrPC.Offset);
#else
                DWORD module_base = SymGetModuleBase(process, frame.AddrPC.Offset);
#endif

                const char * module_name = "[unknown module]";
                if (module_base &&
                        GetModuleFileNameA((HINSTANCE)module_base, module_name_raw, MAX_PATH)) {
                        module_name = module_name_raw;
                        bc = get_bc(ob, set, module_name);
                }

                const char * file = NULL;
                const char * func = NULL;
                unsigned line = 0;

                if (bc) {
                        find(bc,frame.AddrPC.Offset,&file,&func,&line);
                }

                if (file == NULL) {
#ifdef _WIN64
                        DWORD64 dummy = 0;
                        if (SymGetSymFromAddr64(process, frame.AddrPC.Offset, &dummy, symbol))
#else
                        DWORD dummy = 0;
                        if (SymGetSymFromAddr(process, frame.AddrPC.Offset, &dummy, symbol))
#endif
                        {
                                file = symbol->Name;
                        }
                        else {
                                file = "[unknown file]";
                        }
                }
                if (func == NULL) {
                        output_print(ob,"0x%x : %s : %s \n",
                                frame.AddrPC.Offset,
                                module_name,
                                file);
                }
                else {
                        output_print(ob,"0x%x : %s : %s (%d) : in function (%s) \n",
                                frame.AddrPC.Offset,
                                module_name,
                                file,
                                line,
                                func);
                }
        }
}

static char * g_output = NULL;
static LPTOP_LEVEL_EXCEPTION_FILTER g_prev = NULL;

static LONG WINAPI
exception_filter(LPEXCEPTION_POINTERS info)
{
        FILE *f;
        char timestamp[4096];
        char filename[4096];
        time_t rawtime;
        struct tm *ti;

        time(&rawtime);
        ti=localtime(&rawtime);

        sprintf(timestamp,"__%4d_%02d_%02d__%02d_%02d_%02d",
             1900+ti->tm_year,ti->tm_mon+1,ti->tm_mday,
             ti->tm_hour,ti->tm_min,ti->tm_sec);

        sprintf(filename,"backtrace%s.txt",timestamp);
        f=fopen(filename,"wt");

        struct output_buffer ob;
        output_init(&ob, g_output, BUFFER_MAX);

        if (!SymInitialize(GetCurrentProcess(), 0, TRUE)) {
                output_print(&ob,"Failed to init symbol context\n");
        }
        else {
                bfd_init();
                struct bfd_set *set = calloc(1,sizeof(*set));
                _backtrace(&ob , set , 128 , info->ContextRecord);
                release_set(set);

                SymCleanup(GetCurrentProcess());
        }

        fputs(g_output,f);
        fflush(f);
        fclose(f);


        exit(1);

        return 0;
}

static void
backtrace_register(void)
{
        if (g_output == NULL) {
                g_output = malloc(BUFFER_MAX);
                g_prev = SetUnhandledExceptionFilter(exception_filter);
                printf("Backtrace started\n");
        }
}

static void
backtrace_unregister(void)
{
        if (g_output) {
                free(g_output);
                SetUnhandledExceptionFilter(g_prev);
                printf("Backtrace stopped\n");
                g_prev = NULL;
                g_output = NULL;
        }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpvReserved)
{
        switch (dwReason) {
        case DLL_PROCESS_ATTACH:
                backtrace_register();
                break;
        case DLL_PROCESS_DETACH:
                backtrace_unregister();
                break;
        }
        return TRUE;
}

#if defined __GNUC__ || defined __clang__
# define ATTRIBUTE(attrlist) __attribute__(attrlist)
#else
# define ATTRIBUTE(attrlist)
#endif

char *libintl_dgettext (const char *domain_name ATTRIBUTE((unused)), const char *msgid ATTRIBUTE((unused)))
{
    static char buf[1024] = "XXX placeholder XXX";
    return buf;
}

int __printf__ ( const char * format, ... );
int libintl_fprintf ( FILE * stream, const char * format, ... );
int libintl_sprintf ( char * str, const char * format, ... );
int libintl_snprintf ( char *buffer, int buf_size, const char *format, ... );
int libintl_vprintf ( const char * format, va_list arg );
int libintl_vfprintf ( FILE * stream, const char * format, va_list arg );
int libintl_vsprintf ( char * str, const char * format, va_list arg );

int __printf__ ( const char * format, ... )
{
    int value;
    va_list arg;
    va_start(arg, format);
    value = vprintf ( format, arg );
    va_end(arg);
    return value;
}

int libintl_fprintf ( FILE * stream, const char * format, ... )
{
    int value;
    va_list arg;
    va_start(arg, format);
    value = vfprintf ( stream, format, arg );
    va_end(arg);
    return value;
}
int libintl_sprintf ( char * str, const char * format, ... )
{
    int value;
    va_list arg;
    va_start(arg, format);
    value = vsprintf ( str, format, arg );
    va_end(arg);
    return value;
}
int libintl_snprintf ( char *buffer, int buf_size, const char *format, ... )
{
    int value;
    va_list arg;
    va_start(arg, format);
    value = vsnprintf ( buffer, buf_size, format, arg );
    va_end(arg);
    return value;
}
int libintl_vprintf ( const char * format, va_list arg )
{
    return vprintf ( format, arg );
}
int libintl_vfprintf ( FILE * stream, const char * format, va_list arg )
{
    return vfprintf ( stream, format, arg );
}
int libintl_vsprintf ( char * str, const char * format, va_list arg )
{
    return vsprintf ( str, format, arg );
}

/* cut dependence on zlib... libbfd needs this */

int compress (unsigned char *dest ATTRIBUTE((unused)), unsigned long destLen ATTRIBUTE((unused)), const unsigned char source ATTRIBUTE((unused)), unsigned long sourceLen ATTRIBUTE((unused)))
{
    return 0;
}
unsigned long compressBound (unsigned long sourceLen)
{
    return sourceLen + (sourceLen >> 12) + (sourceLen >> 14) + (sourceLen >> 25) + 13;
}
int inflateEnd(void *strm ATTRIBUTE((unused)))
{
    return 0;
}
int inflateInit_(void *strm ATTRIBUTE((unused)), const char *version ATTRIBUTE((unused)), int stream_size ATTRIBUTE((unused)))
{
    return 0;
}
int inflateReset(void *strm ATTRIBUTE((unused)))
{
    return 0;
}
int inflate(void *strm ATTRIBUTE((unused)), int flush ATTRIBUTE((unused)))
{
    return 0;
}

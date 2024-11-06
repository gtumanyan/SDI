#ifndef BaseUtil_h
#define BaseUtil_h

#if defined(_MSC_VER)
#define COMPILER_MSVC 1
#else
#define COMPILER_MSVC 0
#endif

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <wininet.h>
// nasty but necessary
#if defined(min) || defined(max)
#error "min or max defined"
#endif
#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))
#include <gdiplus.h>
#undef NOMINMAX
#undef min
#undef max

// Most common C includes
#include <stdio.h>

// most common c++ includes
#include <cstdint>
#include <memory>
#include <string>

using u8 = uint8_t;
using i32 = int32_t;
using u32 = uint32_t;
using i64 = int64_t;
using u64 = uint64_t;
using uint = unsigned int;

#if COMPILER_MSVC
#define NO_INLINE __declspec(noinline)
#else
// assuming gcc or similar
#define NO_INLINE __attribute__((noinline))
#endif

#define dimof(array) (sizeof(DimofSizeHelper(array)))
#define dimofi(array) (int)(sizeof(DimofSizeHelper(array)))

template <typename T, size_t N>
char (&DimofSizeHelper(T (&array)[N]))[N];

// like dimof minus 1 to account for terminating 0
#define static_strlen(array) (sizeof(DimofSizeHelper(array)) - 1)

// TODO: is there a better way?
#if COMPILER_MSVC
#define IS_UNUSED
#else
#define IS_UNUSED __attribute__((unused))
#endif

#if COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 6011) // silence /analyze: de-referencing a nullptr pointer
#endif
// Note: it's inlined to make it easier on crash reports analyzer (if wasn't inlined
// CrashMe() would show up as the cause of several different crash sites)
//
// Note: I tried doing this via RaiseException(0x40000015, EXCEPTION_NONCONTINUABLE, 0, 0);
// but it seemed to confuse callstack walking
inline void CrashMe() {
    char* p = nullptr;
    // cppcheck-suppress nullPointer
    *p = 0; // NOLINT
}
#if COMPILER_MSVC
#pragma warning(pop)
#endif

// ReportIf() is like assert() except it sends crash report in pre-release and debug
// builds.
// The idea is that assert() indicates "can't possibly happen" situation and if
// it does happen, we would like to fix the underlying cause.
// In practice in our testing we rarely get notified when an assert() is triggered
// and they are disabled in builds running on user's computers.
//
// ReportAlwaysIf() sends a report even in release builds. This is to catch the most
// thorny scenarios.
// Enabling it in pre-release builds but not in release builds is trade-off between
// shipping small executables (each ReportIf() adds few bytes of code) and having
// more testing on user's machines and not only in our personal testing.
// To crash unconditionally use ReportIf(). It should only be used in
// rare cases where we really want to know a given condition happens. Before
// each release we should audit the uses of ReportAlwaysIf()

// in release builds ReportIf()/ReportIfQuick() will break if running under
// the debugger. In other builds it send a debug report
#undef UPLOAD_REPORT
#if defined(PRE_RELEASE_VER) || defined(DEBUG) || defined(ASAN_BUILD)
#define UPLOAD_REPORT
#endif

extern void _uploadDebugReport(const char*, bool, bool);
void BreakIfUnderDebugger();

#ifdef UPLOAD_REPORT
#define ReportIfCond(cond, condStr, isCrash, captureCallstack)      \
    __analysis_assume(!(cond));                                     \
    do {                                                            \
        if (cond) {                                                 \
            _uploadDebugReport(condStr, isCrash, captureCallstack); \
        }                                                           \
    } while (0)
#else
// version that is a no-op
#define ReportIfCond(cond, x, y, z) \
    __analysis_assume(!(cond));     \
    do {                            \
        if (cond) {                 \
            BreakIfUnderDebugger(); \
        }                           \
    } while (0)
#endif

#define ReportIf(cond) ReportIfCond(cond, #cond, false, true)
#define ReportIfQuick(cond) ReportIfCond(cond, #cond, false, false)
#if defined(DEBUG)
#define ReportDebugIf(cond) ReportIfCond(cond, #cond, false, true)
#else
#define ReportDebugIf(cond)
#endif

void* AllocZero(size_t count, size_t size);

template <typename T>
FORCEINLINE T* AllocArray(size_t n) {
    return (T*)AllocZero(n, sizeof(T));
}

template <typename T>
FORCEINLINE T* AllocStruct() {
    return (T*)AllocZero(1, sizeof(T));
}

template <typename T>
inline void ZeroStruct(T* s) {
    ZeroMemory((void*)s, sizeof(T));
}

template <typename T>
inline void ZeroArray(T& a) {
    size_t size = sizeof(a);
    ZeroMemory((void*)&a, size);
}

int limitValue(int val, int min, int max);
DWORD limitValue(DWORD val, DWORD min, DWORD max);
float limitValue(float val, float min, float max);

// return true if adding n to val overflows. Only valid for n > 0
template <typename T>
inline bool addOverflows(T val, T n) {
    ReportIf(!(n > 0));
    T res = val + n;
    return val > res;
}

void* memdup(const void* data, size_t len, size_t extraBytes = 0);
bool memeq(const void* s1, const void* s2, size_t len);

size_t RoundUp(size_t n, size_t rounding);
int RoundUp(int n, int rounding);
char* RoundUp(char*, int rounding);

// Base class for allocators that can be provided to Vec class
// (and potentially others). Needed because e.g. in crash handler
// we want to use Vec but not use standard malloc()/free() functions
struct Allocator {
    Allocator() = default;
    virtual ~Allocator() = default;

    virtual void* Alloc(size_t size) = 0;
    virtual void* Realloc(void* mem, size_t size) = 0;
    virtual void Free(const void* mem) = 0;

    // helper functions that fallback to malloc()/free() if allocator is nullptr
    // helps write clients where allocator is optional
    static void* Alloc(Allocator* a, size_t size);

    template <typename T>
    static T* AllocArray(Allocator* a, size_t n = 1) {
        size_t size = n * sizeof(T);
        return (T*)AllocZero(a, size);
    }

    static void* AllocZero(Allocator* a, size_t size);
    static void Free(Allocator* a, void* p);
    static void* Realloc(Allocator* a, void* mem, size_t size);
    static void* MemDup(Allocator* a, const void* mem, size_t size, size_t extraBytes = 0);
};

// PoolAllocator is for the cases where we need to allocate pieces of memory
// that are meant to be freed together. It simplifies the callers (only need
// to track this object and not all allocated pieces). Allocation and freeing
// is faster. The downside is that free() is a no-op i.e. it can't free memory
// for re-use.
//
// Note: we could be a bit more clever here by allocating data in 4K chunks
// via VirtualAlloc() etc. instead of malloc(), which would lower the overhead
struct PoolAllocator : Allocator {
    // we'll allocate block of the minBlockSize unless
    // asked for a block of bigger size
    size_t minBlockSize = 4096;

    // contains allocated data and index of each allocation
    struct Block {
        struct Block* next;
        size_t dataSize; // size of data in block
        size_t nAllocs;
        // curr points to free space
        char* freeSpace;
        // from the end, we store index of each allocation relative
        // to start of the block. <end> points at the current
        // reverse end of i32 array of indices
        char* end;
        // data follows here
    };

    Block* currBlock = nullptr;
    Block* firstBlock = nullptr;
    int nAllocs = 0;
    CRITICAL_SECTION cs;

    PoolAllocator();

    // Allocator methods
    ~PoolAllocator() override;
    void* Realloc(void* mem, size_t size) override;
    void Free(const void*) override;
    void* Alloc(size_t size) override;

    void FreeAll();
    void Reset(bool poisonFreedMemory = false);
    void* At(int i);

    // only valid for structs, could alloc objects with
    // placement new()
    template <typename T>
    T* AllocStruct() {
        return (T*)Alloc(sizeof(T));
    }

    // Iterator for easily traversing allocated memory as array
    // of values of type T. The caller has to enforce the fact
    // that the values stored are indeed values of T
    // see http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    template <typename T>
    struct Iter {
        PoolAllocator* self;
        int idx;

        // TODO: can make it more efficient
        Iter(PoolAllocator* a, int startIdx) {
            self = a;
            idx = startIdx;
        }

        bool operator!=(const Iter& other) const {
            return idx != other.idx;
        }

        T* operator*() const {
            return (T*)self->At(idx);
        }

        Iter& operator++() {
            idx += 1;
            return *this;
        }
    };

    template <typename T>
    Iter<T> begin() {
        return Iter<T>(this, 0);
    }
    template <typename T>
    Iter<T> end() {
        return Iter<T>(this, nAllocs);
    }
};

struct HeapAllocator : Allocator {
    HANDLE allocHeap = nullptr;

    explicit HeapAllocator(size_t initialSize = 128 * 1024) : allocHeap(HeapCreate(0, initialSize, 0)) {
    }
    ~HeapAllocator() override {
        HeapDestroy(allocHeap);
    }
    void* Alloc(size_t size) override {
        return HeapAlloc(allocHeap, 0, size);
    }
    void* Realloc(void* mem, size_t size) override {
        return HeapReAlloc(allocHeap, 0, mem, size);
    }
    void Free(const void* mem) override {
        HeapFree(allocHeap, 0, (void*)mem);
    }

    HeapAllocator(const HeapAllocator&) = delete;
    HeapAllocator& operator=(const HeapAllocator&) = delete;
};


// from https://pastebin.com/3YvWQa5c
// In my testing, in debug build defer { } creates somewhat bloated code
// but in release it seems to be optimized to optimally small code
#define CONCAT_INTERNAL(x, y) x##y
#define CONCAT(x, y) CONCAT_INTERNAL(x, y)

template <typename T>
struct ExitScope {
    T lambda;
    ExitScope(T lambda) : lambda(lambda) { // NOLINT
    }
    ~ExitScope() {
        lambda();
    }
    ExitScope(const ExitScope&);

  private:
    ExitScope& operator=(const ExitScope&);
};

class ExitScopeHelp {
  public:
    template <typename T>
    ExitScope<T> operator+(T t) {
        return t;
    }
};

// it's 32-bit value which we cast to int for ease of use
struct AtomicInt {
    AtomicInt() = default;
    ~AtomicInt() = default;
    int Set(int n);
    int Inc();
    int Dec();
    int Add(int n);
    int Sub(int n);
    int Get() const;

  private:
    volatile LONG val = 0;
};

using func0Ptr = void (*)(void*);
using funcVoidPtr = void (*)();

#define kFuncNoArg (void*)-1

// the simplest possible function that ties a function and a single argument to it
// we get type safety and convenience with mkFunc()
struct Func0 {
    void* fn = nullptr;
    void* userData = nullptr;

    Func0() = default;
    // copy constructor
    Func0(const Func0& that) {
        this->fn = that.fn;
        this->userData = that.userData;
    }
    // copy assignment operator
    Func0& operator=(const Func0& that) {
        if (this != &that) {
            this->fn = that.fn;
            this->userData = that.userData;
        }
        return *this;
    }
    ~Func0() = default;

    bool IsEmpty() const {
        return fn == nullptr;
    }
    bool IsValid() const {
        return fn != nullptr;
    }
    void Call() const {
        if (!fn) {
            return;
        }
        if (userData == kFuncNoArg) {
            auto func = (funcVoidPtr)fn;
            func();
            return;
        }
        auto func = (func0Ptr)fn;
        func(userData);
    }
};
Func0 MkFunc0Void(funcVoidPtr fn);
#define defer const auto& CONCAT(defer__, __LINE__) = ExitScopeHelp() + [&]()

extern LONG gAllowAllocFailure;

// exists just to mark the intent, needed by both StrUtil.h and TempAllocator.h
using TempStr = char*;
using TempWStr = WCHAR*;

#include "Vec.h"
#include "StrUtil.h"
#include "TempAllocator.h"
#include "StrconvUtil.h"
#include "Scoped.h"

// lstrcpy is dangerous so forbid using it
#ifdef lstrcpy
#undef lstrcpy
#define lstrcpy dont_use_lstrcpy
#endif

#endif

#include "BaseUtil.h"
#include "ScopedWin.h"

// if > 1 we won't crash when memory allocation fails
LONG gAllowAllocFailure = 0;

// returns previous value
int AtomicInt::Set(int n) {
    auto res = InterlockedExchange((LONG*)&val, n);
    return (int)res;
}

// returns value after increment
int AtomicInt::Inc() {
    return (int)InterlockedIncrement(&val);
}

// returns value after decrement
int AtomicInt::Dec() {
    return (int)InterlockedDecrement(&val);
}

// returns value after adding
int AtomicInt::Add(int n) {
    return (int)InterlockedAdd(&val, n);
}

// returns value after subtracting
int AtomicInt::Sub(int n) {
    return (int)InterlockedAdd(&val, -n);
}

int AtomicInt::Get() const {
    return (int)InterlockedCompareExchange((LONG*)&val, 0, 0);
}

void BreakIfUnderDebugger() {
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
}

void* Allocator::Alloc(Allocator* a, size_t size) {
    if (!a) {
        return malloc(size);
    }
    return a->Alloc(size);
}

void* Allocator::AllocZero(Allocator* a, size_t size) {
    void* m = Allocator::Alloc(a, size);
    if (m) {
        ZeroMemory(m, size);
    }
    return m;
}

void Allocator::Free(Allocator* a, void* p) {
    if (!p) {
        return;
    }
    if (!a) {
        free(p);
        return;
    }
    a->Free(p);
}

void* Allocator::Realloc(Allocator* a, void* mem, size_t size) {
    if (!a) {
        return realloc(mem, size);
    }
    return a->Realloc(mem, size);
}

// extraBytes will be zero, useful e.g. for creating zero-terminated strings
// by using extraBytes = sizeof(CHAR)
void* Allocator::MemDup(Allocator* a, const void* mem, size_t size, size_t extraBytes) {
    if (!mem) {
        return nullptr;
    }
    void* newMem = AllocZero(a, size + extraBytes);
    if (newMem) {
        memcpy(newMem, mem, size);
    }
    return newMem;
}

// -------------------------------------

// using the same alignment as windows, to be safe
// TODO: could use the same alignment everywhere but would have to
// align start of the Block, couldn't just start at malloc() address
constexpr size_t kPoolAllocatorAlign = sizeof(char*) * 2;

PoolAllocator::PoolAllocator() {
    InitializeCriticalSection(&cs);
}

void PoolAllocator::Free(const void*) {
    // does nothing, we can't free individual pieces of memory
}

void PoolAllocator::FreeAll() {
    ScopedCritSec scs(&cs);
    Block* curr = firstBlock;
    while (curr) {
        Block* next = curr->next;
        free(curr);
        curr = next;
    }
    currBlock = nullptr;
    firstBlock = nullptr;
    nAllocs = 0;
}

static size_t BlockHeaderSize() {
    return RoundUp(sizeof(PoolAllocator::Block), kPoolAllocatorAlign);
}

// for easier debugging, poison the freed data with 0xdd
// that way if the code tries to used the freed memory,
// it's more likely to crash
static void PoisonData(PoolAllocator::Block* curr) {
    char* d;
    size_t hdrSize = BlockHeaderSize();
    while (curr) {
        // optimization: don't touch memory if there were not allocations
        if (curr->nAllocs > 0) {
            d = (char*)curr + hdrSize;
            // the buffer is big so optimize to only poison the data
            // allocated in this block
            ReportIf(d > curr->freeSpace);
            size_t n = (curr->freeSpace - d);
            const char* dead = "dea_";
            const char* dea0 = "dea\0";
            u32 u32dead = *((u32*)dead);
            u32 u32dea0 = *((u32*)dea0);
            n = n / sizeof(u32);
            u32* w = (u32*)d;
            // fill with "dead", and every 4-th is 0-terminated
            // so that if it's shown in the debugger as a string
            // it shows as "dea_dea_dea_dea"
            for (size_t i = 0; i < n; i++) {
                if ((i & 0x3) == 0x3) {
                    *w++ = u32dea0;
                } else {
                    *w++ = u32dead;
                }
            }
        }
        curr = curr->next;
    }
}

static void ResetBlock(PoolAllocator::Block* block) {
    size_t hdrSize = BlockHeaderSize();
    char* start = (char*)block;
    block->nAllocs = 0;
    block->freeSpace = start + hdrSize;
    block->end = start + block->dataSize;
    block->next = nullptr;
    ReportIf(RoundUp(block->freeSpace, kPoolAllocatorAlign) != block->freeSpace);
}

void PoolAllocator::Reset(bool poisonFreedMemory) {
    ScopedCritSec scs(&cs);
    // free all but first block to
    // allows for more efficient re-use of PoolAllocator
    // with more effort we could preserve all blocks (not sure if worth it)
    Block* first = firstBlock;
    if (!first) {
        ReportIf(currBlock);
        return;
    }
    if (poisonFreedMemory) {
        PoisonData(firstBlock);
    }
    firstBlock = firstBlock->next;
    first->next = nullptr;
    FreeAll();
    ResetBlock(first);
    firstBlock = first;
    currBlock = first;
}

PoolAllocator::~PoolAllocator() {
    FreeAll();
    DeleteCriticalSection(&cs);
}

void* PoolAllocator::Realloc(void*, size_t) {
    // TODO: we can't do that because we don't know the original
    // size of memory piece pointed by mem. We could remember it
    // within the block that we allocate
    CrashMe();
    return nullptr;
}

static void printSize(const char* s, size_t size) {
    char buf[512]{};
    str::BufFmt(buf, dimof(buf), "%s%d\n", s, (int)size);
    OutputDebugStringA(buf);
}

// we allocate the value at the beginning of current block
// and we store a pointer to the value at the end of current block
// that way we can find allocations
void* PoolAllocator::Alloc(size_t size) {
    ScopedCritSec scs(&cs);
    // printSize("PoolAllocator: ", size);

    // need rounded size + space for index at the end
    size_t hdrSize = BlockHeaderSize();
    bool hasSpace = false;
    size_t sizeRounded = RoundUp(size, kPoolAllocatorAlign);
    size_t cbNeeded = sizeRounded + sizeof(i32);
    if (currBlock) {
        ReportIf(currBlock->freeSpace > currBlock->end);
        size_t cbAvail = (currBlock->end - currBlock->freeSpace);
        hasSpace = cbAvail >= cbNeeded;
    }

    if (!hasSpace) {
        cbNeeded += hdrSize;
        size_t dataSize = cbNeeded;
        size_t allocSize = hdrSize + cbNeeded;
        if (allocSize < minBlockSize) {
            allocSize = minBlockSize;
            dataSize = minBlockSize - hdrSize;
        }
        auto block = (Block*)AllocZero(nullptr, allocSize);
        if (!block) {
            return nullptr;
        }
        block->dataSize = dataSize;
        ResetBlock(block);
        if (!firstBlock) {
            ReportIf(currBlock);
            firstBlock = block;
        } else {
            currBlock->next = block;
        }
        currBlock = block;
    }
    char* res = currBlock->freeSpace;
    currBlock->freeSpace = res + sizeRounded;
    if (currBlock->freeSpace > currBlock->end) {
        size_t cbOvershot = currBlock->freeSpace - currBlock->end;
        printSize("PoolAllocator: ", size);
        printSize("overshot: ", cbOvershot);
        printSize("hdrSizet: ", hdrSize);
        ReportIf(true);
    }
    ReportIf(RoundUp(currBlock->freeSpace, kPoolAllocatorAlign) != currBlock->freeSpace);

    char* blockStart = (char*)currBlock;
    i32 offset = (i32)(res - blockStart);
    i32* index = (i32*)currBlock->end;
    index -= 1;
    index[0] = offset;
    currBlock->end = (char*)index;
    currBlock->nAllocs += 1;
    nAllocs += 1;
    return res;
}

// This exits so that I can add temporary instrumentation
// to catch allocations of a given size and it won't cause
// re-compilation of everything caused by changing BaseUtil.h
void* AllocZero(size_t count, size_t size) {
    return calloc(count, size);
}

// extraBytes will be filled with 0. Useful for copying zero-terminated strings
void* memdup(const void* data, size_t len, size_t extraBytes) {
    // to simplify callers, if data is nullptr, ignore the sizes
    if (!data) {
        return nullptr;
    }
    void* dup = AllocZero(len + extraBytes, 1);
    if (dup) {
        memcpy(dup, data, len);
    }
    return dup;
}

bool memeq(const void* s1, const void* s2, size_t len) {
    return 0 == memcmp(s1, s2, len);
}

size_t RoundUp(size_t n, size_t rounding) {
    return ((n + rounding - 1) / rounding) * rounding;
}

int RoundUp(int n, int rounding) {
    if (rounding <= 1) {
        return n;
    }
    return ((n + rounding - 1) / rounding) * rounding;
}

char* RoundUp(char* d, int rounding) {
    if (rounding <= 1) {
        return d;
    }
    uintptr_t n = (uintptr_t)d;
    n = ((n + rounding - 1) / rounding) * rounding;
    return (char*)n;
}


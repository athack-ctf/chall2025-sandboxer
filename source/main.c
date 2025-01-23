// XXX: Move assert header to function definitions that rely on it.
#include <assert.h>

// XXX: Eventually remove once `printf` debugging shenanigans are over.
#include <stdio.h>

#include <WinDef.h>
#include <timeapi.h>
#include <winbase.h>
#include <winuser.h>

#include "global.h"
#include "global_dict.h"
#include "init.h"

typedef struct {
    unsigned long ticksPerS;
    unsigned short minTimerResMs, maxTimerResMs;
} sClockConstant;

typedef struct {
    const sClockConstant c;
    unsigned long long startTicks, endTicks;
    unsigned short loops, sleepMs, curTimerResMs;
} sClock;

static struct {
    unsigned short cpuPermille, handles, pagefileKi, ramKi;
} perfStats;

static sClock constructClock(void);
static unsigned long long getTicks(void);
static unsigned long long getCpuHundredNs(HANDLE hproc);
static void updatePerfStats(HANDLE hproc,
    unsigned int processors, 
    unsigned long long cpuHundredNs,
    unsigned long long clockUs);
#define GET_PERIOD(CLOCK, PRECISION) ( ((PRECISION)*((CLOCK).endTicks-(CLOCK).startTicks)) / (CLOCK).c.ticksPerS )

LRESULT CALLBACK WindowProcedure(HWND hwnd, unsigned int msg, WPARAM wParam,
    LPARAM lParam);

// XXX: Implement a warning report system in a debug log?
int main(void) {
    sClock clock = constructClock();
    HANDLE hproc;
    HWND hwnd;
    unsigned long long prevCpuHundredNs;
    unsigned short prevCpuPermille, peakTimerResMs, processors;
    MirageError code;
    
    // The game loop will end immediately if this function call fails.
    hwnd = constructWindow(&WindowProcedure);
    
    {
        SYSTEM_INFO si[1];
        
        // Initialize to the maximum possible value prevent a  
        // statistic update from occuring immediately.
        prevCpuHundredNs = (unsigned long long) -1;
        prevCpuPermille = 1000;
        peakTimerResMs = clock.c.maxTimerResMs;
        
        // The `GetSystemInfo` function cannot fail.
        GetSystemInfo(si);
        processors = (unsigned short) si->dwNumberOfProcessors;
    }
    
    // The `GetCurrentProcess` function cannot fail. It returns a 
    // constant.
    hproc = GetCurrentProcess();
    
    // The process should not bother if the operating system failed 
    // to set its priority.
    SetPriorityClass(hproc, HIGH_PRIORITY_CLASS);
    
    // The process should not bother if it could not set the timer 
    // resolution.
    timeBeginPeriod(clock.curTimerResMs);
    
    printf("Minimum: %u\nMaximum: %u\n", clock.c.minTimerResMs,
        clock.c.maxTimerResMs);
    
    clock.startTicks = getTicks();
    for (;;) {
        MSG msg[1];
        
        // This branch calculates the amount of time to sleep 
        // frame after sampling one second. Every loop calls some 
        // window update and rendering protocols. Executing these 
        // calls thus incurs time costs every frame. The branch 
        // evaluates the time elapsing between the first and 
        // sixtieth frame. The branch adjusts the sleep time 
        // according to the average milliseconds per frame. The branch 
        // aims to get the average FPS to approach the target FPS.
        if (clock.loops == VIEWPORT_FPS) {
            unsigned long long curCpuHundredNs;
            unsigned int us;
            signed short sleepEstimateMs;
            
            clock.endTicks = getTicks();
            curCpuHundredNs = getCpuHundredNs(hproc);
            us = (unsigned long) GET_PERIOD(clock,1000000UL);
            
            // The current amount of hundreds of nanoseconds may not 
            // vary between subsequent performance measurements.
            if (curCpuHundredNs > prevCpuHundredNs) {
                unsigned long long cpuPeriodUs = curCpuHundredNs 
                    - prevCpuHundredNs;
                unsigned int slowingDown, increasingInCpuTime;
                updatePerfStats(hproc, processors, cpuPeriodUs, us);
                slowingDown = (us+500)/1000 > 1000;
                increasingInCpuTime = perfStats.cpuPermille > prevCpuPermille;
                
                timeEndPeriod(clock.curTimerResMs);
                if (slowingDown && !increasingInCpuTime) {
                    peakTimerResMs = clock.curTimerResMs;
                    clock.curTimerResMs = (unsigned short) 
                        (clock.c.minTimerResMs + clock.curTimerResMs) / 2;
                    
                } else if (!slowingDown && increasingInCpuTime) {
                    clock.curTimerResMs = (unsigned short) 
                        (peakTimerResMs + clock.curTimerResMs) / 2;
                    
                }
                timeBeginPeriod(clock.curTimerResMs);
                
                printf("CPU percent: %u%%\n"
                    "Handle count: %u\n"
                    "Pagefile usage: %uki\n"
                    "RAM usage: %uki\n"
                    "FPS: %u\n"
                    "Sleep time: %i\n"
                    "Timer resolution: %ims\n", perfStats.cpuPermille,
                    perfStats.handles, perfStats.pagefileKi, 
                    perfStats.ramKi, (1000000*VIEWPORT_FPS+1000000/2)/us,
                    clock.sleepMs, clock.curTimerResMs);
                prevCpuPermille = perfStats.cpuPermille;
                
            }
            
            printf("Period: %uus\n", us);
            
            sleepEstimateMs = (signed short) ((2*1000)/VIEWPORT_FPS 
                - (us/1000)/VIEWPORT_FPS
                - clock.curTimerResMs);   // Assume that the sleep 
                                          // period will always be 
                                          // off by the timer 
                                          // resolution quantity.
            if (sleepEstimateMs > 0) {
                clock.sleepMs = (unsigned short) (clock.sleepMs 
                    + sleepEstimateMs) / 2; // Average out the last 
                                            // two sleep periods.
                
            }
            
            clock.loops = 0;
            prevCpuHundredNs = curCpuHundredNs;
            
            // Resume the time for the game logic. Assume that this 
            // branch takes a negligible amount of time to complete.
            clock.startTicks = getTicks();
            
        }
        
        // A sleep time equal to zero milliseconds causes the 
        // thread to yield resources indeterminately. The process 
        // should not sleep in this case instead of yielding resources.
        if (clock.sleepMs > 0) {
            Sleep(clock.sleepMs);
            
        }
        
        // Interpret messages from both the active thread and the 
        // process' window.
        if (PeekMessage(msg, NULL, 0, 0, PM_REMOVE)) {
            unsigned long long procTicksStart = 0; // Uninitializaton
                                                   // cannot be a 
                                                   // problem here. 
                                                   // This 
                                                   // initialization 
                                                   // is only here to 
                                                   // pacify compilers.
            
            // The upper sixteen bits of the `message` member is for 
            // the operating system.
            msg->message &= 0xFFFF;
            if (msg->message == WM_QUIT) {
                code = (MirageError) msg->wParam;
                break;
                
            }
            
            if (msg->message == WM_NCLBUTTONDOWN) {
                procTicksStart = getTicks();
                
            }
            TranslateMessage(msg);
            DispatchMessage(msg);
            
            // Only discard time periods where the user modifies the 
            // window's dimensions. Such modifications first requires 
            // the user to click a non-client area on the window.
            if (msg->message == WM_NCLBUTTONDOWN) {
                clock.startTicks += getTicks()-procTicksStart;
                
                // Do not perform any updates.
                continue;
                
            }
            
        }
        
        // XXX: Update logic here.
        
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE|RDW_UPDATENOW);
        
        clock.loops++;
        
        // Spinlock to reach the frame period to the closest 
        // microsecond.
        do {
            clock.endTicks = getTicks();
        } while (GET_PERIOD(clock,1000000UL) < 
                clock.loops*(1000000UL/VIEWPORT_FPS));
    }
    
    // The process does not bother with the timer resolution 
    // modification failures.
    timeEndPeriod(clock.c.minTimerResMs);
    
    // The operating system automatically unregisters the process'
    // window class after termination.
    return code;
}

#include <wingdi.h>
#include "gfx.h"

LRESULT CALLBACK WindowProcedure(HWND hwnd,
        unsigned int message,
        WPARAM wParam,
        LPARAM lParam) {
    
    // XXX: Someway to not make it static, like a heap alloc? 
    // Incorporate these data as attributes of the window?
    static struct {
        HBITMAP hb;
        HDC memoryDc;
    } backbuffer, atlas;
    
    switch (message) {
        case WM_CREATE: {
            BITMAPINFO bi;
            HDC hdc = GetDC(hwnd);
            if (hdc == NULL) {
                PANIC("The process failed to get the current device context",
                    MIRAGE_INVALID_DC);
                return -1;
                
            }
            
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = VIEWPORT_WIDTH;
            bi.bmiHeader.biHeight = VIEWPORT_HEIGHT;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = VIEWPORT_BPP;
            bi.bmiHeader.biCompression = BI_RGB; 
            bi.bmiHeader.biSizeImage = 0; // The bitmap does not use 
                                          // compression.
            bi.bmiHeader.biXPelsPerMeter = 3809; // Roughly equal to 
                                                 // 96 DPI.
            bi.bmiHeader.biYPelsPerMeter = 3809; // Ditto.
            bi.bmiHeader.biClrUsed = 0;
            bi.bmiHeader.biClrImportant = 0;
            
            // The `CreateCompatibleBitmap` can still take a null 
            // memory device context as input.
            backbuffer.hb = CreateCompatibleBitmap(hdc, VIEWPORT_WIDTH, 
                VIEWPORT_HEIGHT);
            if (backbuffer.hb == NULL) {
                PANIC("The process failed to create the backbuffer.",
                    MIRAGE_BACKBUFFER_INIT_FAIL);
                break;
                
            }
            
            // Create a memory device context for storing the 
            // backbuffer. This device context initially selects a 
            // one-by-one pixel monochrome bitmap.
            backbuffer.memoryDc = CreateCompatibleDC(hdc);
            
            // Select the bitmap for the memory device context into said 
            // device context. The device context will store the 
            // backbuffer bitmap after this function call.
            SelectObject(backbuffer.memoryDc, backbuffer.hb);
            atlas.memoryDc = CreateCompatibleDC(backbuffer.memoryDc);
            atlas.hb = loadGfx(backbuffer.memoryDc, &bi);
            if (atlas.hb == NULL) {
                PANIC("Failed to load the test graphic.",
                    MIRAGE_TEXTURE_INIT_FAIL);
                break;
        
            }
            ReleaseDC(hwnd, hdc);
            
            // Then invoke the default behaviour for the `WM_CREATE`
            // message.
            break;
            
        }
        case WM_KEYDOWN: {
            
            return 0;
        }
        case WM_DESTROY: {
            MirageError code = MIRAGE_OK;
            if (DeleteDC(backbuffer.memoryDc) == 0
                    || DeleteObject(backbuffer.hb) == 0
                    || DeleteDC(atlas.memoryDc) == 0
                    || DeleteObject(atlas.hb) == 0) {
                
                code = MIRAGE_INVALID_FREE;
                
            }
            
            PostQuitMessage(code);
            break;
            
        }
        case WM_PAINT: {
            PAINTSTRUCT ps[1];
            RECT wRect;
            HDC hdc = BeginPaint(hwnd, ps);
            SetStretchBltMode(hdc, COLORONCOLOR); // Makes rendering 
                                                  // less 
                                                  // CPU-intensive.
                                                  // Don't bother if 
                                                  // the function call 
                                                  // fails.
            
            // Render on the backbuffer.
            SelectObject(atlas.memoryDc, atlas.hb);
            if (!BitBlt(backbuffer.memoryDc, 0, 0, 16, 16, 
                    atlas.memoryDc, 0, 0, SRCCOPY)) {
                
                PANIC("The process failed to draw the test bitmap",
                    MIRAGE_BACKBUFFER_WRITE_FAIL);
                break;
                
            }
            
            GetWindowRect(hwnd, &wRect);
            
            // Render on the window.
            if (StretchBlt(hdc, 0, 0, wRect.right-wRect.left, 
                    wRect.bottom-wRect.top, backbuffer.memoryDc, 
                    0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT, SRCCOPY) == 0) {
                
                PANIC("The process failed to refresh the window bitmap.",
                    MIRAGE_WINDOW_WRITE_FAIL);
                break;
                
            }
            
            EndPaint(hwnd, ps);
            
            // The window procedure must return zero after processing 
            // the `WM_PAINT` message.
            return 0;
            
        }
        default: {
            
            break;
            
        }
    }
    
    return DefWindowProc(hwnd, message, wParam, lParam);
    
}

static sClock constructClock(void) {
    union {
        sClockConstant init;
        sClock clock;
    } timeConfig;
    LARGE_INTEGER li;
    TIMECAPS tc[1];
    
    #define MAGIC_SANITYCHECK_CLOCK_TICKSPERSEC ((0x77777777UL<<31)|0x77777777UL)
    timeConfig.init.ticksPerS = MAGIC_SANITYCHECK_CLOCK_TICKSPERSEC;
    assert(timeConfig.clock.c.ticksPerS 
        == MAGIC_SANITYCHECK_CLOCK_TICKSPERSEC);
    #undef MAGIC_SANITYCHECK_CLOCK_TICKSPERSEC
    
    #define MAGIC_SANITYCHECK_CLOCK_MINTIMERRES (0x5555)
    timeConfig.init.minTimerResMs = MAGIC_SANITYCHECK_CLOCK_MINTIMERRES;
    assert(timeConfig.clock.c.minTimerResMs 
        == MAGIC_SANITYCHECK_CLOCK_MINTIMERRES);
    #undef MAGIC_SANITYCHECK_CLOCK_MINTIMERRES
    
    #define MAGIC_SANITYCHECK_CLOCK_MAXTIMERRES (0xAAAA)
    timeConfig.init.maxTimerResMs = MAGIC_SANITYCHECK_CLOCK_MAXTIMERRES;
    assert(timeConfig.init.maxTimerResMs
        == MAGIC_SANITYCHECK_CLOCK_MAXTIMERRES);
    #undef MAGIC_SANITYCHECK_CLOCK_MAXTIMERRES
    
    // The `QueryPerformanceFrequency` function can never fail on 
    // Windows XP and later.
    QueryPerformanceFrequency(&li);
    timeConfig.init.ticksPerS = (unsigned long) li.QuadPart;
    
    if (timeGetDevCaps(tc, sizeof(TIMECAPS)) == MMSYSERR_NOERROR) {
        timeConfig.init.minTimerResMs = (unsigned short) tc->wPeriodMin;
        timeConfig.init.maxTimerResMs = (unsigned short) tc->wPeriodMax;
        if (timeConfig.init.maxTimerResMs > 1000/VIEWPORT_FPS) {
            timeConfig.init.maxTimerResMs = 1000/VIEWPORT_FPS;
            
        }
        
    } else {
        timeConfig.init.minTimerResMs = 1;
        
        // The default timer resolution on Windows is 15.625ms.
        timeConfig.init.maxTimerResMs = 15;
        
    }
    timeConfig.clock.curTimerResMs = (unsigned short)
        (timeConfig.init.minTimerResMs + timeConfig.init.maxTimerResMs) 
        / 2;
    timeConfig.clock.loops = 0;
    timeConfig.clock.sleepMs = 1000 / VIEWPORT_FPS;
    
    return timeConfig.clock;
}

static unsigned long long getCpuHundredNs(HANDLE hproc) {
    FILETIME kernelTime[1], userTime[1], dummy[1];
    ULARGE_INTEGER ia, ib;
    
    GetProcessTimes(hproc, dummy, dummy, kernelTime, userTime);
    ia.LowPart = kernelTime->dwLowDateTime;
    ia.HighPart = kernelTime->dwHighDateTime;
    ib.LowPart = userTime->dwLowDateTime;
    ib.HighPart = userTime->dwHighDateTime;
    return ia.QuadPart+ib.QuadPart;
}

static unsigned long long getTicks(void) {
    LARGE_INTEGER li;
    if (QueryPerformanceCounter(&li) == 0) {
        PANIC("The process could not fetch the current state "
            "of the performance counter.", MIRAGE_INVALID_PERFCOUNTER_ARG);
        return 0;
        
    }
    return (unsigned long long) li.QuadPart;
}

#include <psapi.h>
static void updatePerfStats(HANDLE hproc,
        unsigned int processors, 
        unsigned long long cpuHundredNs,
        unsigned long long clockUs) {
    
    unsigned long handles[1];
    PROCESS_MEMORY_COUNTERS mc[1];
    
    perfStats.cpuPermille = (unsigned short)
        ((100*cpuHundredNs 
        / processors) // This application runs only on one 
                      // core since it only has one thread.
        / clockUs);
    
    // Only update if the function call succeeds.
    if (GetProcessHandleCount(hproc, handles)) {
        perfStats.handles = (unsigned short) *handles;
        
    }
    
    if (GetProcessMemoryInfo(hproc, mc, sizeof mc)) {
        perfStats.pagefileKi = (unsigned short) (mc->PagefileUsage/1024);
        perfStats.ramKi = (unsigned short) (mc->WorkingSetSize/1024);
        
    }
        
    return;
}

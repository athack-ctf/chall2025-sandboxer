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
    unsigned short loops, sleepMs, virtualTimerResMs;
} sClock;

// Note that the debug menu only displays some performance metrics 
// while ignoring others. An example of the latter is the 
// `globalTimerResMs` member.
static struct {
    unsigned short fps, cpuPermille, handles, pagefileKi, ramKi, 
        curTimerResMs, peakTimerResMs, globalTimerResMs;
} perfStats;

static sClock constuctClock(void);
static unsigned long long getTicks(void);
static unsigned long long getCpuHundredNs(HANDLE hproc);
static void updatePerfStats(HANDLE hproc,
    unsigned long long cpuHundredNs,
    unsigned long long clockUs,
    unsigned int processors);
#define GET_PERIOD(CLOCK, PRECISION) ( ((PRECISION)*((CLOCK).endTicks-(CLOCK).startTicks)) / (CLOCK).c.ticksPerS )

LRESULT CALLBACK WindowProcedure(HWND hwnd, unsigned int msg, WPARAM wParam,
    LPARAM lParam);

// XXX: Implement a warning report system in a debug log?
int main(void) {
    sClock clock = constuctClock();
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
    timeBeginPeriod(clock.virtualTimerResMs);
    
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
            unsigned short actualTimerRes;
            
            clock.endTicks = getTicks();
            curCpuHundredNs = getCpuHundredNs(hproc);
            us = (unsigned long) GET_PERIOD(clock,1000000UL);
            
            // The current amount of hundreds of nanoseconds may not 
            // vary between subsequent performance measurements.
            if (curCpuHundredNs > prevCpuHundredNs) {
                unsigned long long cpuPeriodUs = curCpuHundredNs 
                    - prevCpuHundredNs;
                int slowingDown, increasingInCpuTime;
                updatePerfStats(hproc, cpuPeriodUs, us, processors);
                
                // The process is slowing down if the current 
                // framerate is one below the target.
                slowingDown = us/VIEWPORT_FPS > (1000000UL*(VIEWPORT_FPS-1)
                    - VIEWPORT_FPS/2 ) / VIEWPORT_FPS;
                increasingInCpuTime = perfStats.cpuPermille > prevCpuPermille;
                
                timeEndPeriod(clock.virtualTimerResMs);
                if (slowingDown && !increasingInCpuTime) {
                    peakTimerResMs = clock.virtualTimerResMs;
                    clock.virtualTimerResMs = (unsigned short) 
                        (clock.c.minTimerResMs + clock.virtualTimerResMs) / 2;
                    
                } else if (!slowingDown && increasingInCpuTime
                        && perfStats.globalTimerResMs 
                        > clock.virtualTimerResMs) {
                    
                    // Increase the process' timer resolution without 
                    // exceeding the global timer resolution. The 
                    // latter trumps over all requests for higher and 
                    // thus lower precision timer resolutions.
                    clock.virtualTimerResMs = (unsigned short) 
                        (peakTimerResMs + clock.virtualTimerResMs) / 2;
                    
                }
                // Otherwise, do not worry about changing the 
                // current timer resolution from the process' 
                // perspective.
                
                timeBeginPeriod(clock.virtualTimerResMs);
                
                perfStats.fps = (unsigned short)
                    ((1000000UL*VIEWPORT_FPS+1000000UL/2)/us);
                perfStats.peakTimerResMs = peakTimerResMs;
                prevCpuPermille = perfStats.cpuPermille;
                
            }
            
            actualTimerRes = clock.virtualTimerResMs 
                > perfStats.globalTimerResMs ? perfStats.globalTimerResMs 
                : clock.virtualTimerResMs;
            perfStats.curTimerResMs = actualTimerRes;
            
            sleepEstimateMs = (signed short) ((2*1000)/VIEWPORT_FPS 
                - (us/1000) / VIEWPORT_FPS
                - actualTimerRes);  // Assume that the sleep 
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
                
                // Window resizes temporarily increases CPU usage. 
                // Pretending that this increase was a decrease can 
                // avoid decreasing the timer resolution.
                prevCpuHundredNs = (unsigned long long) -1;
                
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

#define DEBUG_METRIC_CHARS 7
#define ARRAY_ELEMENTS(A) (sizeof(A)/(sizeof(*(A))))
#define DEBUG_WIDTH_PELS (VIEWPORT_WIDTH/2)
#define DEBUG_HEIGHT_PELS (VIEWPORT_HEIGHT/3)
#define METRICS 7

LRESULT CALLBACK WindowProcedure(HWND hwnd,
        unsigned int message,
        WPARAM wParam,
        LPARAM lParam) {
    
    // XXX: Someway to not make it static, like a heap alloc? 
    // Incorporate these data as attributes of the window?
    static struct {
        HBITMAP hb;
        HDC memoryDc;
        HFONT hf; // The backbuffer uses a font for the logo.
                  // The atlas uses a font for item tooltips.
                  // The debug interface uses a monospace debug font.
    } backbuffer, atlas, debug;
    static struct {
        
        // Points to an array of string lengths of labels 
        // with a null terminal character.
        const unsigned char *labelPels;
        const char suffix[METRICS][2];
        
        // XXX: Unused value `font.h`
        struct {
            unsigned char w, h;
        } bmp, font;
    } metric = {
        NULL,
        { "  ", "%o", "  ", "ki", "ki", "ms", "ms" },
        { 0, 0 }, { 0, 0 }
    };
    
    switch (message) {
        HDC hdc;
        
        case WM_CREATE: {
            hdc = GetDC(hwnd);
            if (hdc == NULL) {
                PANIC("The process failed to get the current device context",
                    MIRAGE_INVALID_DC);
                break;
                
            }
        }
        // Fall-through
        do {
            BITMAPINFO bi;
            const char debugMetric[] =
                "FPS:\n"
                "CPU Usage:\n"
                "Handle Count:\n"
                "Pagefile Usage:\n"
                "RAM Usage:\n"
                "Current Timer Resolution:\n"
                "Peak Timer Resoltion:\n";
            static unsigned char srcMetricLabelPels[METRICS+1];
            const char fontDebug[] = "Courier New";
            RECT debugRect;
            size_t i, lastIndex, labels, maxLen;
            HFONT oldFont;
            unsigned char debugMenuWidthPels, debugMenuHeightPels,
                fontDebugWidthPels, fontDebugHeightPels;
            
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
            // XXX: Get back the original 1x1 bitmap that the 
            // `SelectObject` function call returns?
            SelectObject(backbuffer.memoryDc, backbuffer.hb);
            atlas.memoryDc = CreateCompatibleDC(backbuffer.memoryDc);
            atlas.hb = loadGfx(backbuffer.memoryDc, &bi);
            if (atlas.hb == NULL) {
                // XXX: Change error message.
                PANIC("Failed to load the test graphic.",
                    MIRAGE_TEXTURE_INIT_FAIL);
                break;
        
            }
            
            for (i = 0, maxLen = 0, lastIndex = 0, labels = 0; 
                    i < ARRAY_ELEMENTS(debugMetric); 
                    ++i) {
                if (debugMetric[i] == '\n') {
                    size_t chars = i-lastIndex;
                    srcMetricLabelPels[labels] = (unsigned char) chars;
                    ++labels;
                    if (chars > maxLen) {
                        maxLen = chars;
                        
                    }
                    lastIndex = i;
                    
                }
            }
            if (labels != METRICS) {
                PANIC("Mismatch between the amount of metrics and "
                    "debug labels.", MIRAGE_DEBUG_METRIC_MISMATCH);
                break;
                
            }
            
            // The last character in the array of string lengths acts 
            // as a terminal.
            srcMetricLabelPels[labels] = 0;
            
            fontDebugWidthPels = (unsigned char)
                (DEBUG_WIDTH_PELS / (maxLen+DEBUG_METRIC_CHARS));
            fontDebugHeightPels = (unsigned char) (DEBUG_HEIGHT_PELS 
                / labels);
            
            for (i = 0; i < labels; ++i) {
                srcMetricLabelPels[i] = (unsigned char)(srcMetricLabelPels[i]
                    *fontDebugWidthPels);
            }
            
            // The pixel data of debug interface is initially a 
            // one-by-one monochrome bitmap.
            debug.memoryDc = CreateCompatibleDC(backbuffer.memoryDc);
            debugMenuWidthPels = (unsigned char)(maxLen*fontDebugWidthPels);
            debug.hb = allocGfx(backbuffer.memoryDc, &bi, debugMenuWidthPels, 
                DEBUG_HEIGHT_PELS);
            if (debug.hb == NULL) {
                PANIC("Failed to load the debug menu pixel data.",
                    MIRAGE_INVALID_DEBUG_PELDATA);
                break;
                
            }
            
            // Create the font for the debug interface.
            debug.hf = CreateFont(fontDebugHeightPels,
                fontDebugWidthPels,
                0, // Escapement
                0, // Orientation
                FW_DONTCARE,
                0, // No italics.
                0, // No underline.
                0, // No strikeout.
                ANSI_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                NONANTIALIASED_QUALITY,
                FIXED_PITCH|FF_DONTCARE,
                fontDebug);
            if (debug.hf == NULL) {
                PANIC("Failed to load the debug font.",
                    MIRAGE_INVALID_FONT);
                break;
                
            }
            
            oldFont = SelectObject(backbuffer.memoryDc, debug.hf);
            #undef HGDI_ERROR
            #define HGDI_ERROR (HFONT)-1
            if (oldFont == NULL || oldFont == HGDI_ERROR) {
                PANIC("Cannot access the debug font.",
                    MIRAGE_LOST_DEBUG_FONT);
                break;
                
            }
            
            // Don't bother if the process could not modify font 
            // attributes.
            SetTextColor(backbuffer.memoryDc, RGB(255, 255, 255));
            SetBkColor(backbuffer.memoryDc, RGB(0, 0, 0));
            
            debugRect.top = 0;
            debugRect.left = 0;
            debugRect.right = DEBUG_WIDTH_PELS;
            debugRect.bottom = DEBUG_HEIGHT_PELS;
            
            // Render text over the backbuffer.
            if (DrawText(backbuffer.memoryDc, 
                    debugMetric, 
                    ARRAY_ELEMENTS(debugMetric), 
                    &debugRect, 
                    DT_LEFT) == 0) {
                PANIC("Debug text render fail.",
                    MIRAGE_CANNOT_RENDER_FONT);
                break;
                
            }
            SelectObject(backbuffer.memoryDc, oldFont);
            SelectObject(backbuffer.memoryDc, backbuffer.hb);
            
            // Assign the debug interface bitmap to the debug memory 
            // device context.
            SelectObject(debug.memoryDc, debug.hb);
            
            debugMenuHeightPels = (unsigned char)(fontDebugHeightPels*labels);
            
            // Write the text data from the backbuffer to the debug 
            // interface's bitmap. This action effectively renders 
            // text to the debug menu's bitmap.
            if (!BitBlt(debug.memoryDc, 0, 0, (int) debugMenuWidthPels, 
                    (int) (fontDebugHeightPels*labels), backbuffer.memoryDc, 
                    0, 0, SRCCOPY)) {
                PANIC("Failed to transfer the backbuffer bitmap's data to "
                    "the debug interface's bitmap.", 
                    MIRAGE_DEBUG_FROM_BACKBUFFER_FAIL);
                break;
                
            }
            
            metric.labelPels = srcMetricLabelPels;
            metric.bmp.w = debugMenuWidthPels;
            metric.bmp.h = debugMenuHeightPels;
            metric.font.w = fontDebugWidthPels;
            metric.font.h = fontDebugHeightPels;
        } while(0);
        // Fall-through
        {
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
            
            // XXX: Delete fonts.
            if (DeleteDC(backbuffer.memoryDc) == 0
                    || DeleteObject(backbuffer.hb) == 0
                    || DeleteDC(atlas.memoryDc) == 0
                    || DeleteObject(atlas.hb) == 0
                    || DeleteObject(debug.hf) == 0) {
                
                code = MIRAGE_INVALID_FREE;
                
            }
            
            PostQuitMessage(code);
            break;
            
        }
        case WM_PAINT: {
            PAINTSTRUCT ps[1];
            RECT wRect;
            HFONT oldFont;
            size_t i;
            const unsigned short *p;
            
            hdc = BeginPaint(hwnd, ps);
            SetStretchBltMode(hdc, COLORONCOLOR); // Makes rendering 
                                                  // less 
                                                  // CPU-intensive.
                                                  // Don't bother if 
                                                  // the function call 
                                                  // fails.
            
            // Render on the backbuffer.
            // XXX: Is it necessary to constantly select things?
            // XXX: Change to properly render sprites from the atlas.
            SelectObject(atlas.memoryDc, atlas.hb);
            if (!BitBlt(backbuffer.memoryDc, 300, 0, 316, 16, 
                    atlas.memoryDc, 0, 0, SRCCOPY)) {
                PANIC("The process failed to draw a sprite.",
                    MIRAGE_BACKBUFFER_WRITE_FAIL);
                break;
                
            }
            
            if (!BitBlt(backbuffer.memoryDc, 0, 0, metric.bmp.w, metric.bmp.h,
                    debug.memoryDc, 0, 0, SRCCOPY)) {
                PANIC("The process failed to render the debug interface",
                    MIRAGE_BACKBUFFER_WRITE_FAIL);
                break;
                
            }
            
            oldFont = SelectObject(backbuffer.memoryDc, debug.hf);
            SetTextColor(backbuffer.memoryDc, RGB(255, 255, 255));
            SetBkColor(backbuffer.memoryDc, RGB(0, 0, 0));
            for (i = 0, p = &perfStats.fps; 
                    i != METRICS; 
                    ++i) {
                unsigned short n = p[i], j = 7 - 2; // Remove suffix
                char buffer[8] = "        ";
                
                memcpy(&buffer[0] + 6, &metric.suffix[i], 
                    sizeof *metric.suffix);
                
                do {
                    buffer[j] = (char)(n%10 + '0');
                    --j;
                } while (n /= 10);
                
                if (!TextOut(backbuffer.memoryDc, 
                        (int) metric.labelPels[i], 
                        (int)(metric.font.h * i) - (int)i,
                        &buffer[j] + 1, 7-j)) {
                    PANIC("The process failed to render the debug interface",
                        MIRAGE_BACKBUFFER_METRIC_FAIL);
                    break;
                    
                }
            }
            SelectObject(backbuffer.memoryDc, oldFont);
            SelectObject(backbuffer.memoryDc, backbuffer.hb);
            
            GetWindowRect(hwnd, &wRect);
            
            // Render on the window.
            if (!StretchBlt(hdc, 0, 0, wRect.right-wRect.left, 
                    wRect.bottom-wRect.top, backbuffer.memoryDc, 
                    0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT, SRCCOPY)) {
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

static sClock constuctClock() {
    sClockConstant init;
    LARGE_INTEGER li;
    TIMECAPS tc[1];
    
    // The `QueryPerformanceFrequency` function can never fail on 
    // Windows XP and later.
    QueryPerformanceFrequency(&li);
    init.ticksPerS = (unsigned long) li.QuadPart;
    
    if (timeGetDevCaps(tc, sizeof(TIMECAPS)) == MMSYSERR_NOERROR) {
        init.minTimerResMs = (unsigned short) tc->wPeriodMin;
        init.maxTimerResMs = (unsigned short) tc->wPeriodMax;
        if (init.maxTimerResMs > 1000/VIEWPORT_FPS) {
            init.maxTimerResMs = 1000/VIEWPORT_FPS;
            
        }
        
    } else {
        // Educated guesses
        init.minTimerResMs = 1;
        init.maxTimerResMs = 1000/VIEWPORT_FPS;
        
    }
    
    {
        sClock clock = { init, 0, 0, 0, 0, 0 };
        
        // The virtual timer resolution is the timer resolution from 
        // the process' perspective. This resolution does not consider 
        // the influence of the global timer resolution.
        clock.virtualTimerResMs = (unsigned short) (clock.c.minTimerResMs 
            + clock.c.maxTimerResMs) / 2;
        clock.loops = 0;
        clock.sleepMs = 1000 / VIEWPORT_FPS;
        return clock;
    }
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
#include <Ntdef.h>
#include <ntstatus.h>
#include <winternl.h>
static void updatePerfStats(HANDLE hproc,
        unsigned long long cpuHundredNs,
        unsigned long long clockUs,
        unsigned int processors) {
    
    unsigned long handles[1], dummy[1], globalTimerResHundredNs;
    PROCESS_MEMORY_COUNTERS mc[1];
    
    perfStats.cpuPermille = (unsigned short)
        ((100*cpuHundredNs 
        / processors) // This application runs only on one 
                      // core since it only has one thread.
        / clockUs);
    
    if (NtQueryTimerResolution(dummy, dummy, &globalTimerResHundredNs)
            == STATUS_SUCCESS) {
        perfStats.globalTimerResMs = (unsigned short)
            (globalTimerResHundredNs / 10000);
        
    }
    
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

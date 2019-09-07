#include "lfdef.h"

volatile long revolve_cycles = 0;

void revolve(int yield)
{
	InterlockedIncrement(&revolve_cycles);
    if(yield != 0)
    {
#ifdef _WIN32
#ifdef _NTDDK_
#else
        Sleep(0);
#endif
#else
        sched_yield();
#endif    
    }
}

void dead_loop()
{
    for(;;)
    {
        revolve(1);
    }
}

long get_revolve_cycles()
{
    return revolve_cycles;
}

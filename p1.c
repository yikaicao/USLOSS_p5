#include "vm.h"
#include "phase5.h"
#include "usloss.h"
#define DEBUG 0

extern int vmInitialized;
extern int debugflag;
extern Process processes[MAXPROC];

void
p1_fork(int pid)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);
} /* p1_fork */


/* As a reminder:
 * In phase 1, p1_switch is called by the dispatcher right before the
 * dispatcher does: enableInterrupts() followed by USLOSS_ContestSwitch()
 */
void
p1_switch(int old, int new)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
    
    if (DEBUG && debugflag)
        USLOSS_Console("p1_switch(): vmInitialized = %d\n", vmInitialized);
    
    if (vmInitialized)
    {
        int i;
        
        // unload old pages
        for (i = 0; i < vmStats.pages; i++)
        {
            if (processes[old].pageTable[i].state == INCORE ||
                processes[old].pageTable[i].state == INDISK)
            {
                USLOSS_MmuUnmap(TAG, i);
            }
        }
        
        // load new pages
        for (i = 0; i < vmStats.pages; i++)
        {
            if (processes[new].pageTable[i].state == INCORE)
                USLOSS_MmuMap(TAG, i, processes[new].pageTable[i].frame, USLOSS_MMU_PROT_RW);
        }
        
        
    }
} /* p1_switch */

void
p1_quit(int pid)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
} /* p1_quit */

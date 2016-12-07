#include "vm.h"
#include "phase5.h"
#include "usloss.h"
#define DEBUG 0

extern int vmInitialized;
extern int debugflag;
extern Process processes[MAXPROC];
extern void printFrameTable();
extern void printPageTable(int pid);

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
        USLOSS_Console("\tp1_switch() called: old = %d, new = %d\n", old, new);
    
    if (DEBUG && debugflag)
        USLOSS_Console("\tp1_switch(): vmInitialized = %d\n", vmInitialized);
    
    if (vmInitialized)
    {
        int i;
        vmStats.switches++;
        
        // unload old pages
        for (i = 0; i < vmStats.pages; i++)
        {
            if (processes[old].pageTable[i].frame != -1)
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

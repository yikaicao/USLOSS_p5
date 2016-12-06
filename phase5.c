/*
 * skeleton.c
 *
 * This is a skeleton for phase5 of the programming assignment. It
 * doesn't do much -- it is just intended to get you started.
 */


#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <usyscall.h>
#include <libuser.h>
#include <vm.h>
#include <string.h>

extern int debugflag;
extern void mbox_create(systemArgs *args_ptr);
extern void mbox_release(systemArgs *args_ptr);
extern void mbox_send(systemArgs *args_ptr);
extern void mbox_receive(systemArgs *args_ptr);
extern void mbox_condsend(systemArgs *args_ptr);
extern void mbox_condreceive(systemArgs *args_ptr);
extern int diskSizeReal(int, int*, int*, int*);

Process processes[MAXPROC];  // phase 5 process table

void *vmRegion; // start of virtual memory frames

FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
VmStats  vmStats;


static void
FaultHandler(int  type,  // USLOSS_MMU_INT
             void *arg); // Offset within VM region

static void vmInit(systemArgs *sysargsPtr);
static void * vmInitReal(int mappings, int pages, int frames, int pagers);
static void vmDestroy(systemArgs *sysargsPtr);
static void vmDestroyReal(void);
static  int  Pager(char *);


// added structures
extern int start5 (char *);
int vmInitialized = 0;
int faultMboxID;
int pagersPID[MAXPAGERS]; // pagersID[i] = -1 for i >= acutal # of pagers
FTE* frameTable;

// added functions
int findFreeFrame();



/*
 *----------------------------------------------------------------------
 *
 * start4 --
 *
 * Initializes the VM system call handlers. 
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int
start4(char *arg)
{
    int pid;
    int result;
    int status;

    /* to get user-process access to mailbox functions */
    systemCallVec[SYS_MBOXCREATE]      = mbox_create;
    systemCallVec[SYS_MBOXRELEASE]     = mbox_release;
    systemCallVec[SYS_MBOXSEND]        = mbox_send;
    systemCallVec[SYS_MBOXRECEIVE]     = mbox_receive;
    systemCallVec[SYS_MBOXCONDSEND]    = mbox_condsend;
    systemCallVec[SYS_MBOXCONDRECEIVE] = mbox_condreceive;

    /* user-process access to VM functions */
    systemCallVec[SYS_VMINIT]    = vmInit;
    systemCallVec[SYS_VMDESTROY] = vmDestroy;

    //  ... more stuff goes here ...

    result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
    if (result != 0) {
        USLOSS_Console("start4(): Error spawning start5\n");
        Terminate(1);
    }

    // Wait for start5 to terminate
    result = Wait(&pid, &status);
    if (result != 0) {
        USLOSS_Console("start4(): Error waiting for start5\n");
        Terminate(1);
    }

    Terminate(0);
    return 0; // not reached

} /* start4 */

/*
 *----------------------------------------------------------------------
 *
 * VmInit --
 *
 * Stub for the VmInit system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is initialized.
 *
 *----------------------------------------------------------------------
 */
static void
vmInit(systemArgs *sysargsPtr)
{
    CheckMode();
    
    // return -2 if already intialized
    if (vmInitialized)
    {
        sysargsPtr->arg4 = (void *) -2;
        return;
    }
    
    // return -1 if given invalid input
    if (sysargsPtr->arg1 != sysargsPtr->arg2)
    {
        sysargsPtr->arg4 = (void *) -1;
        return;
    }
    
    // call real helper
    sysargsPtr->arg1 = vmInitReal((long)sysargsPtr->arg1, (long)sysargsPtr->arg2, (long)sysargsPtr->arg3, (long)sysargsPtr->arg4);
    
    sysargsPtr->arg4 = (void *) 0;
    
    // set flag to indicate intialized
    vmInitialized = 1;
    
} /* vmInit */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroy --
 *
 * Stub for the VmDestroy system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void
vmDestroy(systemArgs *sysargsPtr)
{
    CheckMode();
    vmDestroyReal();
} /* vmDestroy */


/*
 *----------------------------------------------------------------------
 *
 * vmInitReal --
 *
 * Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 *
 * Results:
 *      Address of the VM region.
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
void *
vmInitReal(int mappings, int pages, int frames, int pagers)
{
    int status;
    int dummy;
    int i;

    CheckMode();
    // initialize the MMU
    status = USLOSS_MmuInit(mappings, pages, frames);
    if (status != USLOSS_MMU_OK) {
        USLOSS_Console("vmInitReal: couldn't init MMU, status %d\n", status);
        abort();
    }
    
    // install handler
    USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;


   /*
    * Initialize proc table
    */
    for (i = 0; i < MAXPROC; i++)
    {
        Process *aProc = malloc(sizeof(Process));
        aProc->numPages = 0;
        aProc->pageTable = malloc(pages * sizeof(PTE));
        
        // initialize page table for this process
        int j;
        for (j = 0; j < pages; j++)
        {
            aProc->pageTable[j].state = UNUSED;
            aProc->pageTable[j].frame = -1;
            aProc->pageTable[j].diskBlock = -1;
        }
        
        aProc->privateMboxID = MboxCreate(0, 0);
        processes[i] = *aProc;
    }
    
    /*
     * Initialize frame table
     */
    frameTable = malloc(frames * sizeof(FTE));
    for (i = 0; i < frames; i++)
    {
        frameTable[i].pid = -1;
        frameTable[i].page = -1;
        frameTable[i].referenced = 0;
        frameTable[i].clean = 1;
        frameTable[i].state = UNUSED;
    }
    
    /*
     * Initialize disk table
     */
    int sector, track, disk;
    diskSizeReal(1, &sector, &track, &disk);
    vmStats.diskBlocks = sector * track * disk / USLOSS_MmuPageSize();
    if (debugflag)
    {
        USLOSS_Console("vmInitReal(): sector %d track %d disk %d\n", sector, track, disk);
        USLOSS_Console("vmInitReal(): disk has %d disk blocks\n", vmStats.diskBlocks);
    }
    

   /*
    * Create the fault mailbox.
    */
    faultMboxID = MboxCreate(MAXPROC, sizeof(FaultMsg));

   /*
    * Fork the pagers.
    */
    for (i = 0; i < MAXPAGERS; i++)
    {
        pagersPID[i] = -1;
        if (i < pagers)
        {
            char nameBuf[10];
            sprintf(nameBuf, "Pager%d", i + 1);
            pagersPID[i] = fork1(nameBuf, Pager, NULL, USLOSS_MIN_STACK, PAGER_PRIORITY);
        }
            
    }
    
   /*
    * Zero out, then initialize, the vmStats structure
    */
    memset((char *) &vmStats, 0, sizeof(VmStats));
    vmStats.pages = pages;
    vmStats.frames = frames;
    vmStats.freeFrames = frames;
    vmStats.diskBlocks = sector * track * disk / USLOSS_MmuPageSize();
    vmStats.freeDiskBlocks = sector * track * disk / USLOSS_MmuPageSize();
   /*
    * Initialize other vmStats fields.
    */
    
    return USLOSS_MmuRegion(&dummy);
} /* vmInitReal */


/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the USLOSS_Console.
 *
 *----------------------------------------------------------------------
 */
void
PrintStats(void)
{
     USLOSS_Console("VmStats\n");
     USLOSS_Console("pages:          %d\n", vmStats.pages);
     USLOSS_Console("frames:         %d\n", vmStats.frames);
     USLOSS_Console("diskBlocks:     %d\n", vmStats.diskBlocks);
     USLOSS_Console("freeFrames:     %d\n", vmStats.freeFrames);
     USLOSS_Console("freeDiskBlocks: %d\n", vmStats.freeDiskBlocks);
     USLOSS_Console("switches:       %d\n", vmStats.switches);
     USLOSS_Console("faults:         %d\n", vmStats.faults);
     USLOSS_Console("new:            %d\n", vmStats.new);
     USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
     USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
     USLOSS_Console("replaced:       %d\n", vmStats.replaced);
} /* PrintStats */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroyReal --
 *
 * Called by vmDestroy.
 * Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void
vmDestroyReal(void)
{

   CheckMode();
   USLOSS_MmuDone();
   /*
    * Kill the pagers here.
    */
    int i;
    for (i = 0; i < MAXPAGERS; i++)
    {
        // if this pager exist
        if (pagersPID[i] != -1)
        {
            int pid = getpid();
            faults[pid % MAXPROC].pid = -1; // to indicate zapping pager
            faults[pid % MAXPROC].replyMbox = processes[pid % MAXPROC].privateMboxID;
            MboxSend(faultMboxID, &faults[pid % MAXPROC], sizeof(FaultMsg));
            MboxReceive(processes[pid % MAXPROC].privateMboxID, NULL, 0);
        }
    }
    
   /* 
    * Print vm statistics.
    */
    vmStats.freeFrames = vmStats.frames;
    PrintStats();
   /* and so on... */

} /* vmDestroyReal */

/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 *
 * Results:
 * None.
 *
 * Side effects:
 * The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */
static void
FaultHandler(int  type /* USLOSS_MMU_INT */,
             void *arg  /* Offset within VM region */)
{
    int cause;
    assert(type == USLOSS_MMU_INT);
    cause = USLOSS_MmuGetCause();
    assert(cause == USLOSS_MMU_FAULT);
    vmStats.faults++;
    
   /*
    * Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
    * reply.
    */
    int pid = getpid();
    faults[pid % MAXPROC].pid = pid;
    faults[pid % MAXPROC].addr = arg;
    faults[pid % MAXPROC].replyMbox = processes[pid % MAXPROC].privateMboxID;
    
    MboxSend(faultMboxID, &faults[pid % MAXPROC], sizeof(FaultMsg));
    
    MboxReceive(processes[pid % MAXPROC].privateMboxID, NULL, 0);
    
} /* FaultHandler */

/*
 *----------------------------------------------------------------------
 *
 * Pager 
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int
Pager(char *buf)
{
    while(1) {
        /* Wait for fault to occur (receive from mailbox) */
        FaultMsg aFaultMsg;
        MboxReceive(faultMboxID, &aFaultMsg, sizeof(FaultMsg));
        
        if (debugflag)
            USLOSS_Console("Pager(): received a FaultMsg\n");
        
        if (aFaultMsg.pid == -1)
        {
            MboxSend(aFaultMsg.replyMbox, NULL, 0);
            break;
        }
            
        
        int frameIndex;
        /* Look for free frame */
        frameIndex = findFreeFrame();
        if (frameIndex >= 0)
        {
            if (debugflag)
                USLOSS_Console("Pager(): found a free frame %d\n", frameIndex);
            vmStats.freeFrames--;
        }
        
        /* If there isn't one then use clock algorithm to
         * replace a page (perhaps write to disk) */
        if (frameIndex < 0)
            ;
        
        /* Load page into frame from disk, if necessary */
        
        /* Zero out a new touched page, if necessary */
        // figure out which page this process is using
        int pageIndex = (int) ((long)aFaultMsg.addr / USLOSS_MmuPageSize());
        if (debugflag)
            USLOSS_Console("Pager(): blocked process is trying to access page %d\n", pageIndex);
        // if this page is not touched yet
        if (processes[aFaultMsg.pid % MAXPROC].pageTable[pageIndex].state == UNUSED)
        {
            vmStats.new++;
            if (debugflag)
                USLOSS_Console("Pager(): this page is not used yet\n");
            USLOSS_MmuMap(TAG, pageIndex, frameIndex, USLOSS_MMU_PROT_RW);
            int offset = pageIndex * USLOSS_MmuPageSize();
            int dummy;
            // zero out this page
            memset(USLOSS_MmuRegion(&dummy) + offset, '\0', USLOSS_MmuPageSize());
            USLOSS_MmuUnmap(TAG, pageIndex);
        }
        
        /* Update frame table, page table and process table */
        frameTable[frameIndex].pid = aFaultMsg.pid;
        frameTable[frameIndex].page = pageIndex;
        frameTable[frameIndex].referenced = 0;
        frameTable[frameIndex].clean = 1;
        frameTable[frameIndex].state = INCORE;
        processes[aFaultMsg.pid % MAXPROC].pageTable[pageIndex].state = INCORE;
        processes[aFaultMsg.pid % MAXPROC].pageTable[pageIndex].frame = frameIndex;
        processes[aFaultMsg.pid % MAXPROC].numPages++;
        
        /* Unblock waiting (faulting) process */
        MboxSend(aFaultMsg.replyMbox, NULL, 0);
        
    }
    return 0;
} /* Pager */


/*
 *----------------------------------------------------------------------
 * findFrame
 * 
 * Find a free frame
 *
 * Return index of that frame in the frameTable
 *----------------------------------------------------------------------
 */
int findFreeFrame()
{
    int i;
    
    for (i = 0; i < vmStats.frames; i++)
    {
        if (frameTable[i].state == UNUSED)
            return i;
    }
    return -1;
}


/*
 *----------------------------------------------------------------------
 * printFrameTable
 *----------------------------------------------------------------------
 */
void printFrameTable()
{
    int i;
    for (i = 0; i < vmStats.frames; i++)
    {
        USLOSS_Console("\tprintFrameTable(): frame %d pid %d page %d referenced %d clean %d state %d\n",
                       i, frameTable[i].pid, frameTable[i].page, frameTable[i].referenced, frameTable[i].clean, frameTable[i].state);
    }
}

/*
 *----------------------------------------------------------------------
 * printPageTable
 *----------------------------------------------------------------------
 */
void printPageTable(int pid)
{
    USLOSS_Console("\tprintPageTable(): for process %d, it has:\n", pid);
    
    int i;
    for (i = 0; i < vmStats.pages; i++)
        USLOSS_Console("\t\tpage %d stored at %d in block %d state %d\n", i,
                       processes[pid % MAXPROC].pageTable[i].frame,
                       processes[pid % MAXPROC].pageTable[i].diskBlock,
                       processes[pid % MAXPROC].pageTable[i].state);
}

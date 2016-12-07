/*
 * vm.h
 */

/*
 * All processes use the same tag.
 */
#define TAG 0

/*
 * Different states for a page.
 */
#define UNUSED 500
#define INCORE 501
#define ONDISK 502
#define INCORE_ONDISK 503
#define USED 504 // not unused, not incore, not ondisk
/* You'll probably want more states */


/*
 * Page table entry.
 */
typedef struct PTE {
    int  state;      // See above.
    int  frame;      // Frame that stores the page (if any). -1 if none.
    int  diskBlock;  // Disk block that stores the page (if any). -1 if none.
    // Add more stuff here
} PTE;

/*
 * Frame table entry.
 */
typedef struct FTE {
    int pid;    // which child process is using this frame
    int page;   // which page in a child process's page table is mapped to this entry
    int locked;
} FTE;

/*
 * Disk table entry.
 */
typedef struct DTE {
    int pid;
    int page;
    int state;
} DTE;
#define SLOT_USED 504
#define SLOT_UNUSED 505

/*
 * Per-process information.
 */
typedef struct Process {
    int  numPages;   // Size of the page table.
    PTE  *pageTable; // The page table for the process.
    // Add more stuff here */
    int privateMboxID;
} Process;

/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int  pid;        // Process with the problem.
    void *addr;      // Address that caused the fault.
    int  replyMbox;  // Mailbox to send reply.
    // Add more stuff here.
} FaultMsg;

#define CheckMode() assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)

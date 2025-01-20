// scheduler.cc
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would
//	end up calling FindNextToRun(), and that would put us in an
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------
/*周改的*/
int L1CompareFun(Thread *, Thread *);
int L2CompareFun(Thread *, Thread *);
/*周改的*/
Scheduler::Scheduler()
{
    readyList = new List<Thread *>;
    toBeDestroyed = NULL;

    /*周改的*/
    L1ReadyList = new SortedList<Thread *>(L1CompareFun);
    L2ReadyList = new SortedList<Thread *>(L2CompareFun);
    L3ReadyList = new List<Thread *>;
    /*周改的*/
}

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{
    delete readyList;

    /*周改的*/
    delete L1ReadyList;
    delete L2ReadyList;
    delete L3ReadyList;
    /*周改的*/
}

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void Scheduler::ReadyToRun(Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
    //cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    /*周改的*/
    //cout << thread->getID() << ' ' << thread->GetPriority() << '\n';
    if (thread->CanInL1ReadyList())
    {
        if (!L1ReadyList->IsInList(thread))
            L1ReadyList->Insert(thread);
        DEBUG(dbgList, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[" << 1 << "]");
    }
    else if (thread->CanInL2ReadyList())
    {
        if (!L2ReadyList->IsInList(thread))
            L2ReadyList->Insert(thread);
        DEBUG(dbgList, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[" << 2 << "]");
    }
    else if (thread->CanInL3ReadyList())
    {
        DEBUG(dbgList, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[" << 3 << "]");
        if (!L3ReadyList->IsInList(thread))
        {
            L3ReadyList->Append(thread);
        }
    }

    /*周改的*/

    //readyList->Append(thread);
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    /*周改的*/
    /*if (readyList->IsEmpty())
    {
        return NULL;
    }
    else
    {
        return readyList->RemoveFront();
    }*/
    if (!L1ReadyList->IsEmpty())
    {
        DEBUG(dbgList, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << L1ReadyList->Front()->getID() << "] is removed from queue L[" << 1 << "]");
        return L1ReadyList->RemoveFront();
    }

    else if (!L2ReadyList->IsEmpty())
    {
        DEBUG(dbgList, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << L2ReadyList->Front()->getID() << "] is removed from queue L[" << 2 << "]");
        return L2ReadyList->RemoveFront();
    }

    else if (!L3ReadyList->IsEmpty())
    {
        DEBUG(dbgList, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << L3ReadyList->Front()->getID() << "] is removed from queue L[" << 3 << "]");
        Thread *removeThread = L3ReadyList->RemoveFront();
        //Print();
        return removeThread;
    }

    else
        return NULL;

    /*周改的*/
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void Scheduler::Run(Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;

    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing)
    { // mark that we need to delete current thread
        ASSERT(toBeDestroyed == NULL);
        toBeDestroyed = oldThread;
    }

    if (oldThread->space != NULL)
    {                               // if this thread is a user program,
        oldThread->SaveUserState(); // save the user's CPU registers
        oldThread->space->SaveState();
    }

    oldThread->CheckOverflow(); // check if the old thread
                                // had an undetected stack overflow

    kernel->currentThread = nextThread; // switch to the next thread
    nextThread->setStatus(RUNNING);     // nextThread is now running

    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());

    // This is a machine-dependent assembly language routine defined
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    DEBUG(dbgList, "[E] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID() << "] is now selected for execution, thread [" << oldThread->getID() << "] is replaced, and it has executed [" << oldThread->GetExecutionTime() << "] ticks");
    SWITCH(oldThread, nextThread);

    // we're back, running oldThread

    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed(); // check if thread we were running
                          // before this one has finished
                          // and needs to be cleaned up

    if (oldThread->space != NULL)
    {                                  // if there is an address space
        oldThread->RestoreUserState(); // to restore, do it.
        oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL)
    {
        delete toBeDestroyed;
        toBeDestroyed = NULL;
    }
}

//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void Scheduler::Print()
{
    cout << "Ready list contents:";
    L3ReadyList->Apply(ThreadPrint);
    cout << '\n';
}

/*周改的*/

void Scheduler::AdjustPriority(void)
{

    ListIterator<Thread *> *it1 = new ListIterator<Thread *>(L1ReadyList);
    ListIterator<Thread *> *it2 = new ListIterator<Thread *>(L2ReadyList);
    ListIterator<Thread *> *it3 = new ListIterator<Thread *>(L3ReadyList);

    while (!it1->IsDone())
    {
        ASSERT(it1->Item()->getStatus() == READY);
        it1->Item()->SetWaitingTime(it1->Item()->GetWaitingTime() + 100);
        if (it1->Item()->GetWaitingTime() >= 1500)
        {
            int newPriority = it1->Item()->GetPriority() + 10;
            newPriority = newPriority > 149 ? 149 : newPriority;
            DEBUG(dbgList, "[C] Tick [" << kernel->stats->totalTicks << "]: Thread [" << it1->Item()->getID() << "] changes its priority from [" << it1->Item()->GetPriority() << "] to [" << newPriority << "]");
            it1->Item()->SetPriority(newPriority);
            it1->Item()->SetWaitingTime(0);
        }
        it1->Next();
    }

    while (!it2->IsDone())
    {
        ASSERT(it2->Item()->getStatus() == READY);
        it2->Item()->SetWaitingTime(it2->Item()->GetWaitingTime() + 100);
        if (it2->Item()->GetWaitingTime() >= 1500)
        {
            int newPriority = it2->Item()->GetPriority() + 10;
            newPriority = newPriority > 149 ? 149 : newPriority;
            DEBUG(dbgList, "[C] Tick [" << kernel->stats->totalTicks << "]: Thread [" << it2->Item()->getID() << "] changes its priority from [" << it2->Item()->GetPriority() << "] to [" << newPriority << "]");
            it2->Item()->SetPriority(newPriority);
            Thread *removeThread = it2->Item();
            it2->Next();
            L2ReadyList->Remove(removeThread);
            DEBUG(dbgList, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << removeThread->getID() << "] is removed from queue L[" << 2 << "]");
            ReadyToRun(removeThread);
            continue;
        }
        it2->Next();
    }

    while (!it3->IsDone())
    {
        ASSERT(it3->Item()->getStatus() == READY);
        it3->Item()->SetWaitingTime(it3->Item()->GetWaitingTime() + 100);
        if (it3->Item()->GetWaitingTime() >= 1500)
        {
            int newPriority = it3->Item()->GetPriority() + 10;
            newPriority = newPriority > 149 ? 149 : newPriority;
            DEBUG(dbgList, "[C] Tick [" << kernel->stats->totalTicks << "]: Thread [" << it3->Item()->getID() << "] changes its priority from [" << it3->Item()->GetPriority() << "] to [" << newPriority << "]");
            it3->Item()->SetPriority(newPriority);
            Thread *removeThread = it3->Item();
            it3->Next();
            L3ReadyList->Remove(removeThread);
            DEBUG(dbgList, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << removeThread->getID() << "] is removed from queue L[" << 3 << "]");
            ReadyToRun(removeThread);
            continue;
        }
        it3->Next();
    }
}
int L1CompareFun(Thread *x, Thread *y)
{
    if (x->GetApproximateBurstTime() < y->GetApproximateBurstTime())
        return 1;

    else if (x->GetApproximateBurstTime() > y->GetApproximateBurstTime())
        return -1;
    else
        return 1;

    return 0;
}

int L2CompareFun(Thread *x, Thread *y)
{
    if (x->GetPriority() > y->GetPriority())
        return 1;

    else if (x->GetPriority() < y->GetPriority())
        return -1;
    else
        return 1;

    return 0;
}

/*周改的*/
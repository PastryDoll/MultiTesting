#include <stdio.h>
#include <pthread.h>
#include <unistd.h> // For the sleep function
#include <assert.h>
#include <stdatomic.h>
#include <libkern/OSAtomic.h>
#include <semaphore.h>
#include <dispatch/dispatch.h>
#include <stdatomic.h>



#define NUM_THREADS 8
#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))

struct work_queue_entry
{
    const char *String;
};

struct thread_info
{
    int LogicalThreadIndex;
    dispatch_semaphore_t SemaphoreHandle;
};

static int32_t volatile EntryCount;
static int32_t volatile NextEntryToDo;
pthread_mutex_t counterMutex = PTHREAD_MUTEX_INITIALIZER;

work_queue_entry Entries[256];

#define CompletePastWriteBeforeFutureWrite asm volatile("" : : : "memory");

void PushString(dispatch_semaphore_t semaphore, const char *String){
    assert(EntryCount < ARRAY_COUNT(Entries));
    work_queue_entry *Entry = Entries + EntryCount; //EntryCount++ The increment can happen and a work can use it before the String is pushed
    Entry->String = String;
    CompletePastWriteBeforeFutureWrite // To enforce that it will not reorder
    ++EntryCount; //Solution: but compiler can put this increment anywhere (I tested and it seems that clang doenst change that)
    dispatch_semaphore_signal(semaphore); //Increments the sempahore
}

void* thread_function(void* arg) {

    thread_info *ThreadInfo = (thread_info *)arg;
    for(;;)
    {   
        int32_t  OriginalNextEntry = NextEntryToDo;
        if(OriginalNextEntry < EntryCount)
        {   
            // Before was just a increment.. that means that two thread can enter and make NextEntryTo > EntryCount
            int32_t InitialValue = __sync_val_compare_and_swap(&NextEntryToDo, OriginalNextEntry, OriginalNextEntry+1);
            if (InitialValue == OriginalNextEntry) //If the original is what we thought it should be
            {
                CompletePastWriteBeforeFutureWrite;
                work_queue_entry *Entry = Entries + NextEntryToDo-1;
                printf("ID: %i, Entry: %s, self: %lu, NextEntryToDo: %i, EntryCount %i\n",ThreadInfo->LogicalThreadIndex, Entry->String, (unsigned long)pthread_self(), NextEntryToDo-1, EntryCount);
            }

        }
        else{
            // Wait for the semaphore to be signaled
            dispatch_semaphore_wait(ThreadInfo->SemaphoreHandle, DISPATCH_TIME_FOREVER); //decrements semaphore
        }
    }
    return NULL;
}
int main(){
    pthread_t threads[NUM_THREADS];
    thread_info ThreadInfo[NUM_THREADS];
    dispatch_semaphore_t semaphore;
    semaphore = dispatch_semaphore_create(0);
    for(int ThreadIndex = 0; ThreadIndex < NUM_THREADS; ++ThreadIndex)
    {
        thread_info *Info = ThreadInfo + ThreadIndex;
        Info->SemaphoreHandle = semaphore;
        Info->LogicalThreadIndex = ThreadIndex;
        if (pthread_create(&threads[ThreadIndex], NULL, thread_function, Info) != 0)
        {
            perror("pthread_create");
            return 1;
        }
    }
    PushString(semaphore,"String 0 ");
    PushString(semaphore,"String 1 ");
    PushString(semaphore,"String 2 ");
    PushString(semaphore,"String 3 ");
    PushString(semaphore,"String 4 ");
    PushString(semaphore,"String 5 ");
    PushString(semaphore,"String 6 ");
    PushString(semaphore,"String 7 ");
    PushString(semaphore,"String 8 ");

    usleep(1000000);

    // printf("Threads are finished");
    return 0;
}

// void* thread_function(void* arg) {

//     thread_info *ThreadInfo = (thread_info *)arg;
//     for(;;)
//     {   
//         if(NextEntryToDo < EntryCount)
//         {   
//             __sync_fetch_and_add(&NextEntryToDo,1);
//                 CompletePastWriteBeforeFutureWrite;
//                 work_queue_entry *Entry = Entries + NextEntryToDo-1;
//                 printf("ID: %i, Entry: %s, self: %lu, NextEntryToDo: %i, EntryCount %i\n",ThreadInfo->LogicalThreadIndex, Entry->String, (unsigned long)pthread_self(), NextEntryToDo-1, EntryCount);

//         }
//         else{
//                // Wait for the semaphore to be signaled
//             dispatch_semaphore_wait(ThreadInfo->SemaphoreHandle, DISPATCH_TIME_FOREVER);
//         }
//     }
//     return NULL;
// }

// void* thread_function(void* arg) {
//     thread_info* ThreadInfo = (thread_info*)arg;
//     for (;;) {
//         // usleep(5000000);
//         // int nextEntry = atomic_load(&NextEntryToDo); // Atomically load the current value
//         int nextEntry = NextEntryToDo; // Atomically load the current value
//         if (nextEntry < EntryCount) {
//             if (atomic_compare_exchange_weak(&NextEntryToDo, &nextEntry, nextEntry + 1)) {
//                 // Successfully incremented NextEntryToDo
//                 CompletePastWriteBeforeFutureWrite;
//                 work_queue_entry* Entry = Entries + nextEntry;
//                 printf("ID: %i, Entry: %s, self: %lu, NextEntryToDo: %i, EntryCount %i\n", ThreadInfo->LogicalThreadIndex, Entry->String, (unsigned long)pthread_self(), nextEntry, EntryCount);
//             }
//         } else {
//             // Wait for the semaphore to be signaled
//             dispatch_semaphore_wait(ThreadInfo->SemaphoreHandle, DISPATCH_TIME_FOREVER);
//         }
//     }
//     return NULL;
// }
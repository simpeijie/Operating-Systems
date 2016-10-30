Final Report for Project 1: Threads
===================================

#Task 1: Timer 
Some minor changes were made from the design doc in this section. In the algorithms part of the design doc, we mentioned that the iteration over the list of sleeping threads is done until there are no more threads left in the list. This wasn't really what we intended since the list may not necessarily be empty. When coding this part, we only iterated up to the first encounter of a sleeping thread with `wake_time != timer_ticks ()`. Given that the list was sorted in non-decreasing order of wake times, we are guaranteed not to miss any thread that needs to be woken up. 

The current implementation that we have sleeps a thread and wakes it up by calling `thread_block ()` and `thread_unblock ()`. Our team considered the option of switching to using semaphore to perform this task because semaphore is at a higher level of abstraction compared to both the aforementioned functions. We tried initializing the semaphore of each thread to 0 and calling `sema_down ()` each time a thread is being blocked and calling `sema_up ()` to unblock the thread but to no avail. 

Pei did most of the work here in both design and implementation. 

#Task 2: Priority Scheduler
From our design meeting with Devin we cleared up that each thread could actually hold multiple locks at a time. We also realized that we needed to keep track of a base and a effective priority; in the scheme we described in our design doc we simply set priority to the priority of the thread donating but we didn't think that there was no way to find the original priority. So we set up our implementation to only change the effective priority and not the base priority; once a thread finishes executing to unblock a higher-priority thread we set it to the highest priority out of the threads it is blocking now. 

In `thread struct` we added `lock_wanted` (specified in our original design doc) as well as `locks_held` (which was not specified in the original). We added `locks_held` because we realized a thread could hold multiple threads and we needed to have a way of checking from the thread itself which higher-priority threads it was blocking. 

Pei and Jerry made one version of this and Derek made the final version here. Jerry was mostly  responsible for most of the design of this part.

#Task 3: MLFQS
In this task instead of using 64 queues we had 1 constantly sorted queue, as `ready_queue` was already built into the system. We thought this was a better design choice since 64 queues would require a way to access the correct queue. This way, we could avoid errors stemming from writing more extra code than needed, instead relying on the original infrastructure. We just made sure round-robin was enforced, so we kept moving down the queue if priorities were equal. Derek was mostly responsible for design of this task and a large portion of the implementaion. Jaehyun also had a significant hand in this task. 

Jaehyun helped with general debugging. Jerry was responsible for typing up the final report while taking in input from all other group members. 

#Reflection
Our meetings generally went pretty well--we got a lot done and discussed a lot at most of them. That being said, our meetings usually were scheduled at inopportune times, mostly a result of busy schedules and a lack of foresight and planning. In the future we should schedule for more meetings beforehand rather than asking everyone the day before a meeting is scheduled what times they are free.

We also should start the project earlier. We feel into the trap of finishing the design doc then kind of waited until after it was due to start coding. Everyone then had a series of midterms, which pushed the projects towards the last few days. In the future, we should plan for big time-consuming events like midterms and start earlier. 

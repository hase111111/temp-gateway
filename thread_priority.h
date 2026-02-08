// thread_priority.h
#pragma once
#include <pthread.h>
#include <sched.h>
#include <iostream>

inline void set_fifo_priority(int priority)
{
    sched_param param{};
    param.sched_priority = priority;

    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        std::perror("pthread_setschedparam");
    }
}

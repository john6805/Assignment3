#include "process.h"

Process::Process(ProcessDetails details)
{
    int i;
    pid = details.pid;
    start_time = details.start_time;
    num_bursts = details.num_bursts;
    current_burst = 0;
    burst_times = new uint32_t[num_bursts];
    for (i = 0; i < num_bursts; i++)
    {
        burst_times[i] = details.burst_times[i];
    }
    priority = details.priority;
    state = (start_time == 0) ? Process::State::Ready : Process::State::NotStarted;
    core = -1;
    turn_time = 0;
    wait_time = 0;
    cpu_time = 0;
    remain_time = 0;
    for (i = 0; i < num_bursts; i+=2)
    {
        remain_time += burst_times[i];
    }
}

Process::~Process()
{
    delete[] burst_times;
}

uint16_t Process::GetPid()
{
    return pid;
}

uint32_t Process::GetStartTime()
{
    return start_time;
}

uint8_t Process::GetPriority()
{
    return priority;
}

Process::State Process::GetState()
{
    return state;
}

int8_t Process::GetCpuCore()
{
    return core;
}

double Process::GetTurnaroundTime()
{
    return (double)turn_time / 1000.0;
}

double Process::GetWaitTime()
{
    return (double)wait_time / 1000.0;
}

double Process::GetCpuTime()
{
    return (double)cpu_time / 1000.0;
}

double Process::GetRemainingTime()
{
    return (double)remain_time / 1000.0;
}

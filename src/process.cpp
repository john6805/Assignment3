#include "process.h"
#include "time.h"

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

void Process::SetState(Process::State input)
{
    state = input;
}

uint32_t Process::GetBurstTime()
{
    return burst_times[current_burst];
}

void Process::UpdateCurrentBurst()
{
    current_burst = current_burst + 1;
}

uint32_t Process::GetBurstStartTime()
{
    return burst_start_time;
}

void Process::SetBurstStartTime()
{
    burst_start_time = clock()/1000;
}
int8_t Process::GetCpuCore()
{
    return core;
}

void Process::SetCpuCore(int8_t Core)
{
    core = Core;
    return;
}


double Process::GetTurnaroundTime()
{
    return (double)turn_time / 1000.0;
}

void Process::CalcTurnaroundTime(int32_t time_elapsed)
{
    turn_time = turn_time + time_elapsed;
    return;
}

double Process::GetWaitTime()
{
    return (double)wait_time / 1000.0;
}

void Process::CalcWaitTime(int32_t time_elapsed)
{
    wait_time = wait_time + time_elapsed;
    return;
}

double Process::GetCpuTime()
{
    return (double)cpu_time / 1000.0;
}

void Process::CalcCpuTime(int32_t time_elapsed)
{
    cpu_time = cpu_time + time_elapsed;
    return;
}

double Process::GetRemainingTime()
{
    return (double)remain_time / 1000.0;
}

void Process::SetRemainingTime(int32_t time_elapsed)
{
    remain_time = remain_time - time_elapsed;
    return;
}

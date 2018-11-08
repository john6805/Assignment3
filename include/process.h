#ifndef __PROCESS_H_
#define __PROCESS_H_

#include "configreader.h"

class Process {
public:
    enum State : uint8_t {NotStarted, Ready, Running, IO, Terminated};

private:
    uint16_t pid;
    uint32_t start_time;
    uint16_t num_bursts;
    uint16_t current_burst;
    uint32_t *burst_times;
    uint32_t burst_start_time;
    uint8_t priority;
    State state;
    int8_t core;
    int32_t turn_time;
    int32_t wait_time;
    int32_t cpu_time;
    int32_t remain_time;

public:
    Process(ProcessDetails details);
    ~Process();

    uint16_t GetPid();
    uint32_t GetStartTime();
    uint8_t GetPriority();
    State GetState();
    void SetState(Process::State input);
    uint32_t GetBurstTime();
    uint32_t GetBurstStartTime();
    void SetBurstStartTime();
    void UpdateCurrentBurst();
    int8_t GetCpuCore();
    void SetCpuCore(int8_t Core);
    double GetTurnaroundTime();
    void CalcTurnaroundTime();
    double GetWaitTime();
    void CalcWaitTime();
    double GetCpuTime();
    void CalcCpuTime();
    double GetRemainingTime();
    void SetRemainingTime(int32_t newRemainTime);
};

#endif // __PROCESS_H_

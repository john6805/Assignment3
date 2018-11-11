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
    std::chrono::high_resolution_clock::time_point burst_start_time;
    uint32_t burst_elapsed;
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
    std::chrono::high_resolution_clock::time_point GetBurstStartTime();
    void SetBurstStartTime();
    void UpdateCurrentBurst();
    int8_t GetCpuCore();
    void SetCpuCore(int8_t Core);
    double GetTurnaroundTime();
    void CalcTurnaroundTime(int32_t time_elapsed);
    double GetWaitTime();
    void CalcWaitTime(int32_t time_elapsed);
    double GetCpuTime();
    void CalcCpuTime(int32_t time_elapsed);
    double GetRemainingTime();
    void SetRemainingTime(int32_t time_elapsed);
    uint32_t GetBurstElapsed();
    void SetBurstElapsed(uint32_t time_elapsed);
};

#endif // __PROCESS_H_

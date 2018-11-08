#include <iostream>
#include <string>
#include <list>
#include <thread>
#include <mutex>
#include <vector>
#include "configreader.h"
#include "process.h"
#include "time.h"

void ScheduleProcesses(uint8_t core_id, ScheduleAlgorithm algorithm, uint32_t context_switch, uint32_t time_slice,
                       std::list<Process*> *ready_queue, std::mutex *mutex);

int main(int argc, char **argv)
{
    // Ensure user entered a command line parameter for configuration file name
    if (argc < 2)
    {
        std::cerr << "Error: must specify configuration file" << std::endl;
        exit(1);
    }

    // Read configuration file for scheduling simulation
    SchedulerConfig *config;
    ReadConfigFile(argv[1], &config);

    // Store configuration parameters and create processes 
    int i;
    uint8_t cores = config->cores;
    ScheduleAlgorithm algorithm = config->algorithm;
    uint32_t context_switch = config->context_switch;
    uint32_t time_slice = config->time_slice;
    std::vector<Process*> processes;
    std::list<Process*> ready_queue;
    for (i = 0; i < config->num_processes; i++)
    {
        Process *p = new Process(config->processes[i]);
        processes.push_back(p);
        if (p->GetState() == Process::State::Ready)
        {
            ready_queue.push_back(p);
        }
    }

    // Free configuration data from memory
    DeleteConfig(&config);

    //start timer
    std::clock_t start_time;
    std::clock_t current_time;
    start_time = clock();
    // Launch 1 scheduling thread per cpu core
    std::mutex mutex;
    std::thread *schedule_threads = new std::thread[cores];
    for (i = 0; i < cores; i++)
    {
        schedule_threads[i] = std::thread(ScheduleProcesses, i, algorithm, context_switch, time_slice, &ready_queue, &mutex);
    }

    // Main thread work goes here:
    //      While(not all terminated)
    int terminated = 0;
    while(terminated != processes.size())
    {
        terminated = 0;
        for(int i = 0; i < processes.size(); i++)
        {
            current_time = clock();
            if(processes[i]->GetState() == Process::State::Terminated)
            {
                terminated++;
            }
            if (processes[i]->GetState() == Process::State::NotStarted && (current_time - start_time) >= processes[i]->GetStartTime())
            {
                processes[i]->SetState(Process::State::Ready);
                ready_queue.push_back(processes[i]);
            }
            else if(processes[i]->GetState() == Process::State::IO) 
            {
                if(processes[i]->GetBurstTime() <= current_time - start_time)
                {
                    processes[i]->SetState(Process::State::Ready);
                    processes[i]->UpdateCurrentBurst();
                    ready_queue.push_back(processes[i]);
                }
            }
        }
        //PrintStatistics()
    }
    //          Check state of each process, if not started, check start time and start
    //          if in io check io time and add to ready
    //  - Start new processes at their appropriate start time
    //  - Determine when an I/O burst finishes and put the process back in the ready queue
    //  - Sort the ready queue (based on scheduling algorithm)
    //  - Output process status table


    // Wait for threads to finish
    for (i = 0; i < cores; i++)
    {
        schedule_threads[i].join();
    }

    //std::cout << calcCPUUtil() << "\n";
    // Print final statistics
    //  - CPU utilization
    //  - Throughput
    //     - Average for first 50% of processes finished
    //     - Average for second 50% of processes finished
    //     - Overall average
    //  - Average turnaround time
    //  - Average waiting time


    // Clean up before quitting program
    processes.clear();

    return 0;
}

void ScheduleProcesses(uint8_t core_id, ScheduleAlgorithm algorithm, uint32_t context_switch, uint32_t time_slice,
                       std::list<Process*> *ready_queue, std::mutex *mutex)
{
    // Work to be done by each core idependent of the other cores
    //  - Get process at front of ready queue
    //  - Simulate the processes running until one of the following:
    //     - CPU burst time has elapsed
    //     - RR time slice has elapsed
    //     - Process preempted by higher priority process
    //  - Place the process back in the appropriate queue
    //     - I/O queue if CPU burst finished (and process not finished)
    //     - Terminated if CPU burst finished and no more bursts remain
    //     - Ready queue if time slice elapsed or process was preempted
    //  - Wait context switching time
    //  - Repeat until all processes in terminated state
}

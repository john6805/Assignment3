#include <iostream>
#include <string>
#include <list>
#include <thread>
#include <mutex>
#include <vector>
#include <unistd.h>
#include "configreader.h"
#include "process.h"
#include "time.h"

void ScheduleProcesses(uint8_t core_id, ScheduleAlgorithm algorithm, uint32_t context_switch, uint32_t time_slice,
                       std::list<Process*> *ready_queue, std::mutex *mutex);
int PrintStatistics(std::vector<Process*> processes, ScheduleAlgorithm algorithm);
void PPInsert(std::list<Process*> *ready_queue, Process* currentProcess);
void SJFInsert(std::list<Process*> *ready_queue, Process* currentProcess);

//global variables
bool processesTerminated = false;

int main(int argc, char **argv)
{
    time_t start, end;
    time (&start);
    
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
            if(algorithm == ScheduleAlgorithm::SJF)
            {
                SJFInsert(&ready_queue, p);
            }
            else if(algorithm == ScheduleAlgorithm::PP)
            {
                PPInsert(&ready_queue, p);
            }
            else
            {
                ready_queue.push_back(p);
            }         
        }
    }
    // Free configuration data from memory
    DeleteConfig(&config);

    int linesPrinted = PrintStatistics(processes, algorithm);
    //start timer
    std::clock_t start_time;
    std::clock_t current_time;
    std::clock_t time_elapsed;
    start_time = clock() / 1000;
    
    // Launch 1 scheduling thread per cpu core
    std::mutex mutex;
    std::thread *schedule_threads = new std::thread[cores];
    
    for (i = 0; i < cores; i++)
    {
        schedule_threads[i] = std::thread(ScheduleProcesses, i, algorithm, context_switch, time_slice, &ready_queue, &mutex);
    }

    // Main thread work goes here:
    int terminated = 0;
    while(terminated < processes.size())
    {
        terminated = 0;
        for(int i = 0; i < processes.size(); i++)
        {
            current_time = clock() / 1000;
            //usleep(1);
            if(processes[i]->GetState() == Process::State::Terminated)
            {
                terminated++;
            }
            if (processes[i]->GetState() == Process::State::NotStarted && (current_time - start_time) >= processes[i]->GetStartTime())
            {
                processes[i]->SetState(Process::State::Ready);
                if(algorithm == ScheduleAlgorithm::SJF)
                {
                    SJFInsert(&ready_queue, processes[i]);
                }
                else if(algorithm == ScheduleAlgorithm::PP)
                {
                    PPInsert(&ready_queue, processes[i]);
                }
                else
                {
                    ready_queue.push_back(processes[i]);
                }    
            }
            else if(processes[i]->GetState() == Process::State::IO) 
            {
                //time_elapsed is not working (value is too small)
                time_elapsed = (clock()/1000) - current_time;
                processes[i]->CalcTurnaroundTime(time_elapsed);
                if((current_time - processes[i]->GetBurstStartTime()) >= processes[i]->GetBurstTime())
                {
                    processes[i]->SetState(Process::State::Ready);
                    processes[i]->UpdateCurrentBurst();
                    if(algorithm == ScheduleAlgorithm::SJF)
                    {
                        SJFInsert(&ready_queue, processes[i]);
                    }
                    else if(algorithm == ScheduleAlgorithm::PP)
                    {
                        PPInsert(&ready_queue, processes[i]);
                    }
                    else
                    {
                        ready_queue.push_back(processes[i]);
                    }                 
                }
            }
            else if(processes[i]->GetState() == Process::State::Ready)
            {
                //time_elapsed is not working (value is too small)
                time_elapsed = clock()/1000 - current_time;
                processes[i]->CalcWaitTime(1);
                processes[i]->CalcTurnaroundTime(1);
            }      
        }
        //sort ready queue based on scheduling algorithm


        for (int i=0; i<linesPrinted; i++) {
            fputs("\033[A\033[2K", stdout);
        }
        rewind(stdout);
        linesPrinted = PrintStatistics(processes, algorithm);
        usleep(100000);
    }
    processesTerminated = true;
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
    Process* currentProcess;
    uint32_t start;
    uint32_t end;
    uint32_t time_elapsed;
    uint32_t burst_elapsed = 0;
    uint32_t burst_time;
    while(!processesTerminated)
    {
        if(algorithm == ScheduleAlgorithm::FCFS || algorithm == ScheduleAlgorithm::SJF)
        {
            mutex->lock();
            //Get process at front of ready queue
            if(!ready_queue->empty())
            {
                currentProcess = ready_queue->front();
                ready_queue->pop_front();
                mutex->unlock();
                currentProcess->SetCpuCore(core_id);
                burst_time = currentProcess->GetBurstTime();
                burst_elapsed = 0;
                while(burst_elapsed < burst_time && currentProcess->GetRemainingTime() > 0)
                {
                    //Simulate Process running
                    start = clock();
                    currentProcess->SetState(Process::State::Running);
                    end = clock();
                    time_elapsed = (end/1000) - (start/1000);
                    currentProcess->SetRemainingTime(time_elapsed);
                    currentProcess->CalcTurnaroundTime(time_elapsed);
                    currentProcess->CalcCpuTime(time_elapsed);
                    burst_elapsed = burst_elapsed + time_elapsed;
                }
                currentProcess->UpdateCurrentBurst();
                //Place process back in queue
                if(currentProcess->GetRemainingTime() <= 0)
                {
                    currentProcess->SetCpuCore(-1);
                    currentProcess->SetState(Process::State::Terminated);
                    
                }
                else
                {
                    currentProcess->SetCpuCore(-1);
                    currentProcess->SetState(Process::State::IO);
                    currentProcess->SetBurstStartTime();
                }
                //wait context switching time
                usleep(context_switch);
            }

            if(algorithm == ScheduleAlgorithm::PP)
            {
                mutex->lock();
                //Get process at front of ready queue
                if(!ready_queue->empty())
                {
                    currentProcess = ready_queue->front();
                    ready_queue->pop_front();
                    mutex->unlock();
                    currentProcess->SetCpuCore(core_id);
                    burst_time = currentProcess->GetBurstTime();
                    burst_elapsed = currentProcess->GetBurstElapsed();
                    while(burst_elapsed < burst_time && currentProcess->GetRemainingTime() > 0)
                    {
                        //Simulate Process running
                        start = clock();
                        currentProcess->SetState(Process::State::Running);
                        end = clock();
                        time_elapsed = (end/1000) - (start/1000);
                        currentProcess->SetRemainingTime(time_elapsed);
                        currentProcess->CalcTurnaroundTime(time_elapsed);
                        currentProcess->CalcCpuTime(time_elapsed);
                        burst_elapsed = burst_elapsed + time_elapsed;
                        mutex->lock();
                        if(!ready_queue->empty() && ready_queue->front()->GetPriority() > currentProcess->GetPriority())
                        {
                            //Put process in ready queue then pop front of ready queue
                            currentProcess->SetBurstElapsed(burst_elapsed);
                            PPInsert(ready_queue, currentProcess);
                            currentProcess = ready_queue->front();
                            ready_queue->pop_front();
                            mutex->unlock();
                            currentProcess->SetCpuCore(core_id);
                            burst_time = currentProcess->GetBurstTime();
                            burst_elapsed = currentProcess->GetBurstElapsed();
                        }
                    }
                    currentProcess->UpdateCurrentBurst();
                    currentProcess->SetBurstElapsed(0);
                    //Place process back in queue
                    if(!ready_queue->empty() && ready_queue->front()->GetPriority() > currentProcess->GetPriority())
                    {
                        PPInsert(ready_queue, currentProcess);
                    }
                    if(currentProcess->GetRemainingTime() <= 0)
                    {
                        currentProcess->SetCpuCore(-1);
                        currentProcess->SetState(Process::State::Terminated);
                    }
                    else
                    {
                        currentProcess->SetCpuCore(-1);
                        currentProcess->SetState(Process::State::IO);
                        currentProcess->SetBurstStartTime();
                    }
                    //wait context switching time
                    usleep(context_switch);
                }

                //if(Round Robin)
                //if(Shortest Job First)
                //if(Premptive Priority)
            }
            else 
            {
                mutex->unlock();
            }
        }
        
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
    // Work to be done by each core idependent of the other core
       //  - 
}

int PrintStatistics(std::vector<Process*> processes, ScheduleAlgorithm algorithm) {
    int linesPrinted = 2;
    std::cout << "|   PID | Priority |       State | Core |  Turn Time |  Wait Time |   CPU Time | Remain Time |\n";
    std::cout << "+-------+----------+-------------+------+------------+------------+------------+-------------+\n";
    for(int i = 0; i<processes.size(); i++){
        if (processes[i]->GetState() != Process::State::NotStarted) {
            linesPrinted++;
            std::string processLine = "";
            std::string pid = std::to_string(processes[i]->GetPid());
            std::string priority = std::to_string(processes[i]->GetPriority());
            std::string core = std::to_string(processes[i]->GetCpuCore());
            std::string turnTime = std::to_string(processes[i]->GetTurnaroundTime());
            std::string waitTime = std::to_string(processes[i]->GetWaitTime());
            std::string cpuTime = std::to_string(processes[i]->GetCpuTime());
            std::string remTime = std::to_string(processes[i]->GetRemainingTime());
            processLine += "|       |          |             |      |            |            |            |             |\n";

            //PID
            int k = 6;
            for(int j = pid.length()-1; j >= 0; j--) {
                processLine[k] = pid[j];
                k--;
            }
            //Priority
            if(algorithm == ScheduleAlgorithm::PP) {
                processLine[17] = priority[0];
            }
            else {
                processLine[17] = '0';
            }
            //State
            if(processes[i]->GetState() == Process::State::Ready) {
                processLine[27] = 'r';
                processLine[28] = 'e';
                processLine[29] = 'a';
                processLine[30] = 'd';
                processLine[31] = 'y';
            }
            else if(processes[i]->GetState() == Process::State::Running) {
                processLine[25] = 'r';
                processLine[26] = 'u';
                processLine[27] = 'n';
                processLine[28] = 'n';
                processLine[29] = 'i';
                processLine[30] = 'n';
                processLine[31] = 'g';
            }
            else if(processes[i]->GetState() == Process::State::IO) {
                processLine[29] = 'i';
                processLine[30] = '/';
                processLine[31] = 'o';
            }
            else if(processes[i]->GetState() == Process::State::Terminated){
                processLine[22] = 't';
                processLine[23] = 'e';
                processLine[24] = 'r';
                processLine[25] = 'm';
                processLine[26] = 'i';
                processLine[27] = 'n';
                processLine[28] = 'a';
                processLine[29] = 't';
                processLine[30] = 'e';
                processLine[31] = 'd';
            }
            //Core
            if(core != "-1") {
                processLine[38] = core[0];
            }
            else{
                processLine[37] = '-';
                processLine[38] = '-';
            }
            //Turn Time
            k = 51;
            for(int j = turnTime.length()-1; j >= 0; j--) {
                processLine[k] = turnTime[j];
                k--;
            }
            //Wait Time
            k = 64;
            for(int j = turnTime.length()-1; j >= 0; j--) {
                processLine[k] = waitTime[j];
                k--;
            }
            //CPU Time
            k = 77;
            for(int j = turnTime.length()-1; j >= 0; j--) {
                processLine[k] = cpuTime[j];
                k--;
            }
            //Remain Time
            k = 90;
            for(int j = remTime.length()-1; j >= 0; j--) {
                processLine[k] = remTime[j];
                k--;
            }
            /*//PID
            std::cout << "|       |";
            //Priority
            std::cout << "          |";
            //State
            std::cout << "            |";
            //Core
            std::cout << "   -- |";
            //Turn Time
            std::cout << "           |";
            //Wait Time
            std::cout << "           |";
            //CPU Time
            std::cout << "         |";
            //Remain Time
            std::cout << "             |\n";*/
            std::cout << processLine;
        }
    }
    return linesPrinted;
}

void SJFInsert(std::list<Process*> *ready_queue, Process* currentProcess)
{
    std::list<Process*>::iterator it;
    for(it = ready_queue->begin(); it != ready_queue->end(); ++it)
    {
        if(currentProcess->GetRemainingTime() < (*it)->GetRemainingTime())
        {
            ready_queue->insert(it, currentProcess);
            return;
        }
    }
    if(it == ready_queue->end())
    {
        ready_queue->push_back(currentProcess);
    }
}

void PPInsert(std::list<Process*> *ready_queue, Process* currentProcess)
{
    std::list<Process*>::iterator it;
    for(it = ready_queue->begin(); it != ready_queue->end(); ++it)
    {
        if(currentProcess->GetPriority() < (*it)->GetPriority())
        {
            ready_queue->insert(it, currentProcess);
            return;
        }
    }
    if(it == ready_queue->end())
    {
        ready_queue->push_back(currentProcess);
    }
}

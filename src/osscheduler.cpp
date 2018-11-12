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
double printTurnTime(std::vector<Process*> processes);
double printWaitTime(std::vector<Process*> processes);

//global variables
bool processesTerminated = false;
double CPUUtilCore[4];

int main(int argc, char **argv)
{
    std::chrono::high_resolution_clock timer;
    
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
            processes[i]->SetReadyQueueEntryTime(timer.now());
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

    //PrintStatistics(processes, algorithm);
    int linesPrinted = PrintStatistics(processes, algorithm);
    //start timer
    
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point current_time;
    std::chrono::duration<double> time_elapsed;
    std::chrono::duration<double> time_since_start;
    double throughputFirstHalf = 0.0;
    double throughputSecondHalf = 0.0;
    double timeHalf = 0.0;
    double time2ndHalf = 0.0;
    int flag = 0;
    
    // Launch 1 scheduling thread per cpu core
    std::mutex mutex;
    std::thread *schedule_threads = new std::thread[cores];
    
    for (i = 0; i < cores; i++)
    {
        schedule_threads[i] = std::thread(ScheduleProcesses, i, algorithm, context_switch, time_slice, &ready_queue, &mutex);
    }
    usleep(1000);
    // Main thread work goes here:
    int terminated = 0;
    start_time = timer.now();
    while(terminated < processes.size())
    {
        terminated = 0;
        for(int i = 0; i < processes.size(); i++)
        {
            current_time = timer.now();
            time_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(current_time - start_time);
            time_since_start = std::chrono::duration_cast<std::chrono::duration<double>>(current_time - processes[i]->GetProcessStartTime());
            if(processes[i]->GetState() == Process::State::Terminated)
            {
                terminated++;
                if(terminated == processes.size()/2 && flag == 0) {
                    timeHalf = (time_elapsed.count());
                    throughputFirstHalf = (terminated)/timeHalf;
                    flag++;
                }
            }
            else
            {
                processes[i]->CalcTurnaroundTime(time_since_start.count() * 1000);
            }
            
            if (processes[i]->GetState() == Process::State::NotStarted && (time_elapsed.count() * 1000) >= processes[i]->GetStartTime())
            {
                processes[i]->SetState(Process::State::Ready);
                processes[i]->SetReadyQueueEntryTime(timer.now());
                processes[i]->SetProcessStartTime();
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
                time_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(current_time - processes[i]->GetBurstStartTime());
                if((time_elapsed.count() * 1000) >= processes[i]->GetBurstTime())
                {
                    processes[i]->SetState(Process::State::Ready);
                    processes[i]->UpdateCurrentBurst();
                    processes[i]->SetReadyQueueEntryTime(timer.now());
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
                time_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(current_time - processes[i]->GetReadyQueueEntryTime());
                processes[i]->CalcWaitTime(time_elapsed.count() * 1000);
                processes[i]->SetReadyQueueEntryTime(timer.now());
            }      
        }
        //sort ready queue based on scheduling algorithm


        for (int i=0; i<linesPrinted; i++) {
            fputs("\033[A\033[2K", stdout);
        }
        rewind(stdout);
        linesPrinted = PrintStatistics(processes, algorithm);
        //to allow for refresh
        usleep(100000);
    }
    processesTerminated = true;
    current_time = timer.now();
    time_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(current_time - start_time);
    time2ndHalf = (time_elapsed.count()) - timeHalf;
    throughputSecondHalf = (processes.size()-processes.size()/2)/time2ndHalf;
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

    double avgCpuUtil = 0.0;
    for (i = 0; i < cores; i++)
    {
        avgCpuUtil += CPUUtilCore[i];
    }
    
    avgCpuUtil = (avgCpuUtil / cores);
    std::cout << "CPU Utilization: " << avgCpuUtil << "%\n";
    std::cout << "Average Throughput for First Half: " << throughputFirstHalf << "\n";
    std::cout << "Average Throughput for Second Half: " << throughputSecondHalf << "\n";
    std::cout << "Average Throughput: " << processes.size()/(time2ndHalf+timeHalf) << "\n";
    std::cout << "Average Turnaround Time: " << printTurnTime(processes) << "\n";
    std::cout << "Average Wait Time: " << printWaitTime(processes) << "\n";
    
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
    std::chrono::high_resolution_clock timer;
    std::chrono::high_resolution_clock::time_point start;
    std::chrono::high_resolution_clock::time_point end;
    std::chrono::duration<double> time_elapsed;
    uint32_t slice_elapsed;
    uint32_t burst_elapsed = 0;
    uint32_t burst_time;
    std::chrono::high_resolution_clock::time_point before;
    std::chrono::high_resolution_clock::time_point after;
    std::chrono::high_resolution_clock::time_point threadstarted = timer.now();
    std::chrono::duration<double> cpuUtil;
    
    while(!processesTerminated)
    {
        if(algorithm == ScheduleAlgorithm::FCFS || algorithm == ScheduleAlgorithm::SJF)
        {
            mutex->lock();
            //Get process at front of ready queue
            if(!ready_queue->empty())
            {
                before = timer.now();
                start = timer.now();
                currentProcess = ready_queue->front();
                ready_queue->pop_front();
                mutex->unlock();
                currentProcess->SetCpuCore(core_id);
                burst_time = currentProcess->GetBurstTime();
                burst_elapsed = 0;
                while(burst_elapsed < burst_time && currentProcess->GetRemainingTime() > 0)
                {
                    //Simulate Process running
                    usleep(1000);
                    currentProcess->SetState(Process::State::Running);
                    end = timer.now();
                    time_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
                    currentProcess->SetRemainingTime(time_elapsed.count() * 1000);
                    start = timer.now();
                    currentProcess->CalcCpuTime(time_elapsed.count() * 1000);
                    burst_elapsed = burst_elapsed + (time_elapsed.count() * 1000);
                    
                }
                currentProcess->UpdateCurrentBurst();
                if(currentProcess->GetRemainingTime() <= 0)
                {
                    //update CPU utilization for this core
                    after = timer.now();
                    cpuUtil += std::chrono::duration_cast<std::chrono::duration<double>>(after-before);
                    //update process status information
                    currentProcess->SetCpuCore(-1);
                    currentProcess->SetState(Process::State::Terminated);
                    
                }
                else
                {
                    //update CPU utilization for this core
                    after = timer.now();
                    cpuUtil += std::chrono::duration_cast<std::chrono::duration<double>>(after-before);
                    //update process status information
                    currentProcess->SetCpuCore(-1);
                    currentProcess->SetState(Process::State::IO);
                    currentProcess->SetBurstStartTime();
                    //wait context switching time
                    usleep(context_switch);
                }    
            }          
            else 
            {
                mutex->unlock();
            }
        }
        else if(algorithm == ScheduleAlgorithm::PP)
        {
            mutex->lock();
            //Get process at front of ready queue
            if(!ready_queue->empty())
            {
                before = timer.now();
                currentProcess = ready_queue->front();
                ready_queue->pop_front();
                mutex->unlock();
                currentProcess->SetCpuCore(core_id);
                burst_time = currentProcess->GetBurstTime();
                burst_elapsed = currentProcess->GetBurstElapsed();
                while(burst_elapsed < burst_time && currentProcess->GetRemainingTime() > 0)
                {
                    //Simulate Process running
                    start = timer.now();
                    usleep(1000);
                    currentProcess->SetState(Process::State::Running);
                    end = timer.now();
                    time_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
                    currentProcess->SetRemainingTime(time_elapsed.count() * 1000);
                    currentProcess->CalcCpuTime(time_elapsed.count() * 1000);
                    burst_elapsed = burst_elapsed + (time_elapsed.count() * 1000);
                    currentProcess->SetBurstElapsed(time_elapsed.count() * 1000);
                    mutex->lock();
                    if(!ready_queue->empty() && ready_queue->front()->GetPriority() < currentProcess->GetPriority())
                    {
                        //Put process in ready queue then pop front of ready queue
                        
                        currentProcess->SetCpuCore(-1);
                        currentProcess->SetState(Process::State::Ready);
                        PPInsert(ready_queue, currentProcess);
                        currentProcess = ready_queue->front();
                        ready_queue->pop_front();
                        mutex->unlock();
                        
                        after = timer.now();
                        cpuUtil += std::chrono::duration_cast<std::chrono::duration<double>>(after-before);
                        //wait context switching time
                        usleep(context_switch);
                        before = timer.now();
                        currentProcess->SetCpuCore(core_id);
                        burst_time = currentProcess->GetBurstTime();
                        burst_elapsed = currentProcess->GetBurstElapsed();
                    }
                    else 
                    {
                        mutex->unlock();
                    }
                }
                currentProcess->UpdateCurrentBurst();
                currentProcess->SetBurstElapsed(currentProcess->GetBurstElapsed() * -1);
                if(currentProcess->GetRemainingTime() <= 0)
                {
                    //update CPU utilization for this core
                    after = timer.now();
                    cpuUtil += std::chrono::duration_cast<std::chrono::duration<double>>(after-before);
                    //update process status information
                    currentProcess->SetCpuCore(-1);
                    currentProcess->SetState(Process::State::Terminated);
                }
                else
                {
                    //update CPU utilization for this core
                    after = timer.now();
                    cpuUtil += std::chrono::duration_cast<std::chrono::duration<double>>(after-before);
                    //update process status information
                    currentProcess->SetCpuCore(-1);
                    currentProcess->SetState(Process::State::IO);
                    currentProcess->SetBurstStartTime();
                    //wait context switching time
                    usleep(context_switch);
                }
            }
            else
            {
                mutex->unlock();
            }
        }
        else if(algorithm == ScheduleAlgorithm::RR)
        {
            mutex->lock();
            //Get process at front of ready queue
            if(!ready_queue->empty())
            {
                before = timer.now();
                currentProcess = ready_queue->front();
                ready_queue->pop_front();
                mutex->unlock();
                currentProcess->SetCpuCore(core_id);
                burst_time = currentProcess->GetBurstTime();
                burst_elapsed = 0;
                while(currentProcess->GetBurstElapsed() < burst_time && currentProcess->GetRemainingTime() > 0)
                {
                    //Simulate Process running
                    currentProcess->SetState(Process::State::Running);
                    start = timer.now();
                    usleep(1000);
                    end = timer.now();
                    time_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
                    currentProcess->SetRemainingTime(time_elapsed.count() * 1000);
                    currentProcess->CalcCpuTime(time_elapsed.count() * 1000);
                    burst_elapsed = burst_elapsed + (time_elapsed.count() * 1000);
                    currentProcess->SetBurstElapsed(time_elapsed.count() * 1000);
                    if(burst_elapsed > time_slice)
                    {
                        currentProcess->SetState(Process::State::Ready);
                        currentProcess->SetCpuCore(-1);
                        after = timer.now();
                        cpuUtil += std::chrono::duration_cast<std::chrono::duration<double>>(after-before);
                        
                        //Perform context switch
                        usleep(context_switch);
                        mutex->lock();
                        ready_queue->push_back(currentProcess);
                        currentProcess = ready_queue->front();
                        ready_queue->pop_front();
                        mutex->unlock();
                        before = timer.now();

                        currentProcess->SetCpuCore(core_id);
                        burst_time = currentProcess->GetBurstTime();
                        burst_elapsed = 0;
                    }
                }
                currentProcess->UpdateCurrentBurst();
                currentProcess->SetBurstElapsed(currentProcess->GetBurstElapsed() * -1);
                if(currentProcess->GetRemainingTime() <= 0)
                {
                    //update CPU utilization for this core
                    after = timer.now();
                    cpuUtil += std::chrono::duration_cast<std::chrono::duration<double>>(after-before);
                    //update process status information
                    currentProcess->SetCpuCore(-1);
                    currentProcess->SetState(Process::State::Terminated);
                    after = timer.now();
                }
                else
                {
                    //update CPU utilization for this core
                    after = timer.now();
                    cpuUtil += std::chrono::duration_cast<std::chrono::duration<double>>(after-before);
                    //update process status information
                    currentProcess->SetCpuCore(-1);
                    currentProcess->SetState(Process::State::IO);
                    currentProcess->SetBurstStartTime();
                    //wait context switching time
                    usleep(context_switch);
                }
            }
            else
            {
                mutex->unlock();
            }
        }
    }
        
    after = timer.now();
    time_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(after - threadstarted);
    
    if (core_id == 0) {
    CPUUtilCore[0] = (cpuUtil.count()/time_elapsed.count())*100;
    }
    else if (core_id == 1) {
    CPUUtilCore[1] = (cpuUtil.count()/time_elapsed.count())*100;
    }
    else if (core_id == 2) {
    CPUUtilCore[2] = (cpuUtil.count()/time_elapsed.count())*100;
    }
    else if (core_id == 3) {
    CPUUtilCore[3] = (cpuUtil.count()/time_elapsed.count())*100;
    }
    return;
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
            processLine = "|       |          |             |      |            |            |            |             |\n";

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
            for(int j = waitTime.length()-1; j >= 0; j--) {
                processLine[k] = waitTime[j];
                k--;
            }
            //CPU Time
            k = 77;
            for(int j = cpuTime.length()-1; j >= 0; j--) {
                processLine[k] = cpuTime[j];
                k--;
            }
            //Remain Time
            k = 91;
            for(int j = remTime.length()-1; j >= 0; j--) {
                processLine[k] = remTime[j];
                k--;
            }
            //print current processes line
            std::cout << processLine;
        }
    }
    //linesPrinted used to know how many line to remove
    return linesPrinted;
}

double printTurnTime(std::vector<Process*> processes) {
    double avgTurnTime = 0.0;
    for (int i = 0; i < processes.size(); i++) {
        avgTurnTime += processes[i]->GetTurnaroundTime();
    }
    return avgTurnTime/processes.size();
}

double printWaitTime(std::vector<Process*> processes) {
    double avgWaitTime = 0.0;
    for (int i = 0; i < processes.size(); i++) {
        avgWaitTime += processes[i]->GetWaitTime();
    }
    return avgWaitTime/processes.size();
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
    return;
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
    return;
}

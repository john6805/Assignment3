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
    time_t start, end;
    time (&start);
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
            //processes[i]->SetProcessStartTime();
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
            //auto current_time_s = std::chrono::time_point_cast<std::chrono::seconds>(current_time);
            //auto value = current_time_s.time_since_epoch();
            time_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(current_time - start_time);
            time_since_start = std::chrono::duration_cast<std::chrono::duration<double>>(current_time - processes[i]->GetProcessStartTime());
            if(processes[i]->GetState() == Process::State::Terminated)
            {
                terminated++;
                if(terminated == processes.size()/2) {
                    timeHalf = (time_elapsed.count());
                    throughputFirstHalf = (terminated)/timeHalf;
                }
                /*else if (terminated == processes.size()) {
                    time2ndHalf = current_time - start_time - timeHalf;
                    throughputSecondHalf = (terminated*1.0-processes.size()/2)/(time2ndHalf/1000.0);
                }*/
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
                //time_elapsed is not working (value is too small)
                time_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(current_time - processes[i]->GetBurstStartTime());
                //processes[i]->CalcTurnaroundTime(time_elapsed.count() * 1000);
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
                //time_elapsed is not working (value is too small)
                time_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(current_time - processes[i]->GetReadyQueueEntryTime());
                processes[i]->CalcWaitTime(time_elapsed.count() * 1000);
                processes[i]->SetReadyQueueEntryTime(timer.now());
                //processes[i]->CalcTurnaroundTime(time_elapsed.count() * 1000);
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
    current_time = timer.now();
    time_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(current_time - start_time);
    time2ndHalf = (time_elapsed.count() * 1000) - timeHalf;
    //throughputSecondHalf = (processes.size()-processes.size()/2)/time2ndHalf;
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
    
    auto start_ms = std::chrono::time_point_cast<std::chrono::seconds>(start_time);
    auto start_value = start_ms.time_since_epoch();
    
    //auto timeHalf_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(timeHalf);
    //auto timeHalf_value = timeHalf_ms.time_since_epoch();
    
    avgCpuUtil = (avgCpuUtil / cores);
    std::cout << "Average CPU Utilization: " << avgCpuUtil << "%\n";
    std::cout << "Time Start: " << start_value.count() << "\n";
    std::cout << "Time Half: " << timeHalf << "\n";
    std::cout << "# of processes: " << processes.size()/2 << "\n";
    std::cout << "Throughput First Half: " << throughputFirstHalf << "\n";
    // std::cout << "Current time: " << current_time << "\n";
    std::cout << "Time 2nd Half: " << time2ndHalf << "\n";
    std::cout << "# of processes: " << processes.size()-processes.size()/2 << "\n";
    std::cout << "Throughput Second Half: " << throughputSecondHalf << "\n";
    std::cout << "Average Throughput: " << processes.size()/time2ndHalf << "\n";
    // std::cout << "Average Turnaround Time: " << printTurnTime(processes) << "\n";
    // std::cout << "Average Wait Time: " << printWaitTime(processes) << "\n";
    
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
                    //currentProcess->CalcTurnaroundTime(time_elapsed.count() * 1000);
                    currentProcess->CalcCpuTime(time_elapsed.count() * 1000);
                    burst_elapsed = burst_elapsed + (time_elapsed.count() * 1000);
                    
                }
                currentProcess->UpdateCurrentBurst();
                //Place process back in queue
                if(currentProcess->GetRemainingTime() <= 0)
                {
                    after = timer.now();
                    cpuUtil += std::chrono::duration_cast<std::chrono::duration<double>>(after-before);
                    currentProcess->SetCpuCore(-1);
                    currentProcess->SetState(Process::State::Terminated);
                    
                }
                else
                {
                    after = timer.now();
                    cpuUtil += std::chrono::duration_cast<std::chrono::duration<double>>(after-before);
                    currentProcess->SetCpuCore(-1);
                    
                    //wait context switching time
                    usleep(context_switch);
                    currentProcess->SetState(Process::State::IO);
                    currentProcess->SetBurstStartTime();
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
                    //currentProcess->CalcTurnaroundTime(time_elapsed.count() * 1000);
                    currentProcess->CalcCpuTime(time_elapsed.count() * 1000);
                    burst_elapsed = burst_elapsed + (time_elapsed.count() * 1000);
                    mutex->lock();
                    if(!ready_queue->empty() && ready_queue->front()->GetPriority() < currentProcess->GetPriority())
                    {
                        //Put process in ready queue then pop front of ready queue
                        currentProcess->SetBurstElapsed(burst_elapsed);
                        currentProcess->SetCpuCore(-1);
                        currentProcess->SetState(Process::State::Ready);
                        PPInsert(ready_queue, currentProcess);
                        currentProcess = ready_queue->front();
                        ready_queue->pop_front();
                        mutex->unlock();
                        //wait context switching time
                        after = timer.now();
                        cpuUtil += std::chrono::duration_cast<std::chrono::duration<double>>(after-before);
                        usleep(context_switch);
                        currentProcess->SetCpuCore(core_id);
                        burst_time = currentProcess->GetBurstTime();
                        burst_elapsed = currentProcess->GetBurstElapsed();
                        start = timer.now();
                    }
                    else {
                        mutex->unlock();
                    }
                }
                currentProcess->UpdateCurrentBurst();
                currentProcess->SetBurstElapsed(0);
                if(currentProcess->GetRemainingTime() <= 0)
                {
                    after = timer.now();
                    cpuUtil += std::chrono::duration_cast<std::chrono::duration<double>>(after-before);
                    currentProcess->SetCpuCore(-1);
                    currentProcess->SetState(Process::State::Terminated);
                    after = timer.now();
                }
                else
                {
                    after = timer.now();
                    cpuUtil += std::chrono::duration_cast<std::chrono::duration<double>>(after-before);
                    currentProcess->SetCpuCore(-1);
                    currentProcess->SetState(Process::State::IO);
                    currentProcess->SetBurstStartTime();
                    //wait context switching time
                    usleep(context_switch);
                }
            }

            //if(Round Robin)
            //if(Shortest Job First)
            //if(Premptive Priority)
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
    after = timer.now();
    time_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(after - threadstarted);
    //auto after_s = std::chrono::time_point_cast<std::chrono::milliseconds>(after);
    //auto value = after_s.time_since_epoch();
    
    if (core_id == 0) {
    //std::cout << "Core 0, CPU time: " << cpuUtil.count() << "\n";
    std::cout << "Time running: " << time_elapsed.count() << "\n";
    //std::cout << "Core 0, CPU Util: " << (cpuUtil.count()/time_elapsed.count())*100 << "%\n";
    //std::cout << "Core 0, CPU Util: " << (cpuUtil.count()/(time_elapsed.count()*1000))*100 << "%\n";
    CPUUtilCore[0] = (cpuUtil.count()/time_elapsed.count())*100;
    }
    else if (core_id == 1) {
    //std::cout << "Core 1, CPU: " << cpuUtil.count() << "\n";
    //std::cout << "Core 1, CPU Util: " << (cpuUtil.count()/time_elapsed.count())*100 << "%\n";
    CPUUtilCore[1] = (cpuUtil.count()/time_elapsed.count())*100;
    }
    else if (core_id == 2) {
    //std::cout << "Core 2, CPU: " << cpuUtil << "\n";
    //std::cout << "Core 2, CPU Util: " << (cpuUtil/after)*100 << "%\n";
    CPUUtilCore[2] = (cpuUtil.count()/time_elapsed.count())*100;
    }
    else if (core_id == 3) {
    //std::cout << "Core 3, CPU: " << cpuUtil << "\n";
    //std::cout << "Core 3, CPU Util: " << (cpuUtil/after)*100 << "%\n";
    CPUUtilCore[3] = (cpuUtil.count()/time_elapsed.count())*100;
    }
    return;
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

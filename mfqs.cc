// -*- lsst-c++ -*-
//
// Multilevel Feedback Queue Scheduling (MFQS or MLFQ)
// Written by BJH (https://github.com/BJHCode) for CS 452 Operating Systems in 2022 
//
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.

#include <vector>
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <queue>
#include <cmath>
#include <iomanip>
#include <chrono>

// Process Struct
struct process{
    int pid;
    int burst_time;
    int total_burst;
    int last_burst;
    int arrival_time;
    int start_time = -1;
    int waiting_time = 0;  
    int end_time; 
    int turnaround_time; // End - Start
};

// Vaidate File Extension 
// Adapted With Changes From Dietrich Epp Under A CC BY-SA 3.0 License 
// https://stackoverflow.com/questions/20446201/how-to-check-if-string-ends-with-txt
// https://creativecommons.org/licenses/by-sa/3.0/
bool has_suffix(const std::string &str, const std::string &suffix)
{
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Format Time
std::string format_duration( std::chrono::milliseconds ms ) {
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(ms);
    ms -= std::chrono::duration_cast<std::chrono::milliseconds>(secs);
    auto mins = std::chrono::duration_cast<std::chrono::minutes>(secs);
    secs -= std::chrono::duration_cast<std::chrono::seconds>(mins);
    auto hour = std::chrono::duration_cast<std::chrono::hours>(mins);
    mins -= std::chrono::duration_cast<std::chrono::minutes>(hour);

    std::stringstream ss;
    ss << hour.count() << " Hours : " << mins.count() << " Minutes : " << secs.count() << " Seconds : " << ms.count() << " Milliseconds";
    return ss.str();
}

// Process Arrival Time Comparision 
bool arrival_sort(const process& a, const process& b){
    return a.arrival_time < b.arrival_time;
}

// Transform Given Test File Into Processes Vector
std::vector<process> file_to_vector(std::string fname){
    
    // Vector Of Processes
    std::vector<process> processes;

    // Read File Stream
    std::ifstream input(fname);

    // Catch Error
    if (!input)
    {
        std::cout << "Error Opening File." << std::endl;
        exit (EXIT_FAILURE);
    }

    // Store Line Variable
    std::string line;

    // Skip First (Header) Line
    std::getline(input, line);

    // Read Lines, Split Into Tokens, Generate Process, And Add It To Vector 
    while(std::getline(input, line)){
        struct process pro;
        std::vector<std::string> tokens;
        std::stringstream ss(line);
        std::string token;
        while (ss >> token) {
            tokens.push_back(token);
        }
        pro.pid = stoi(tokens.at(0));
        pro.burst_time = stoi(tokens.at(1));
        pro.total_burst = pro.burst_time;
        pro.arrival_time = stoi(tokens.at(2));
        if(pro.burst_time > 0 && pro.arrival_time >= 0){
            processes.push_back(pro);
        }  
    }

    // Sort By Arrival Time
    std::stable_sort(processes.begin(), processes.end(), arrival_sort);

    return processes;
}

int main(int argc, char *argv[]) {
    std::cout << "MFQS By BJH\n\n";

    // Declare Variables
    int time = 0, complete = 0, iter = 0, apid = -1, age = 0;
    const int time_quant_mult = 2;
    const bool to_file = false; // True If Reporting To File
    double avg_turnaround = 0, avg_waiting = 0;

    // Handle Command Line Test File
    if(argc != 2){
        std::cout << "A test file is required and should be the only command line argument." << std::endl;
        return 1;
    }
    std::string test_file(argv[1]);
    if(!has_suffix(test_file, ".txt")){
        std::cout << "The text file extension must be .txt." << std::endl;
        return 1;
    }    

    // Get User Input
    int queue_num = 0, aging = 0, time_quant = 0;
    while(queue_num < 2){
        std::cout << "Enter the number of queues: ";
        std::cin >> queue_num;
        if(queue_num < 2){
            std::cout << "Please enter a number greater than 2: ";
        }
    }
    while(time_quant < 1){
        std::cout << "Enter time quantum for top queue: ";
        std::cin >> time_quant;
        if(time_quant < 1){
            std::cout << "Please enter a number greater than 0: ";
        }
    }
    while(aging < 1){
        std::cout << "Enter aging interval: ";
        std::cin >> aging;
        if(aging < 1){
            std::cout << "Please enter a number greater than 0: ";
        }
    }    
    std::cout << "\n"; // Formatting

    // Start Timer
    auto start = std::chrono::high_resolution_clock::now();

    // If True, Write Report To File
    std::ofstream myfile;
    if(to_file){
        myfile.open ("Report.txt");
        myfile << "MFQS By BJH\n\n" << "Number of Queues: " << queue_num << "\nFirst Time Quantum: " << time_quant << "\nAging Interval: " << aging << "\n\n" << "Reading File... ";
    }
    // Process File
    std::cout << "Reading File... " << std::flush;
    std::vector<process> processes = file_to_vector(test_file);

    // Formatting 
    size_t width = floor(log10(processes.size()) + 1);
    size_t wid = 13;

    // Create Vector of Process Queue
    std::vector<std::queue<process>> multilevel(queue_num);
    
    // FIFO Queue Position
    int fifo = queue_num - 1;

    // Create Vector Of Queue Time Quantums
    std::vector<int> quantum;
    for(int i = 0; i < fifo; i++){
        quantum.push_back(time_quant * pow(time_quant_mult, i));
    }

    // Run MFQS
    if(to_file){myfile << "Running MFQS...\n" << std::endl;}
    std::cout << "Running MFQS...\n" << std::endl;
    while(complete < processes.size()){

        // Push Processes To Queue While Arrival Time == Time
        while(processes[iter].arrival_time == time){
            // Report
            if(to_file){myfile << "Process " << std::setw(width) << std::right << processes[iter].pid << std::setw(wid) << std::left << " Arrives @ " << time << "\n";}
            else{std::cout << "Process " << std::setw(width) << std::right << processes[iter].pid << std::setw(wid) << std::left << " Arrives @ " << time << "\n";}
            // Arrive
            multilevel[0].push(processes[iter]);
            iter++;
        }

        // If FIFO Is Not Empty
        if(!multilevel[fifo].empty()){
            // Increment Age
            age++;
            // If Aged
            if (age == aging){
                // Reset Age
                age = 0;
                // Report
                if(to_file){myfile << "Process " << std::setw(width) << std::right << multilevel[fifo].front().pid << std::setw(wid) << std::left << " Promoted @ " << time << "\n";}
                else{std::cout << "Process " << std::setw(width) << std::right << multilevel[fifo].front().pid << std::setw(wid) << std::left << " Promoted @ " << time << "\n";}
                // Promote
                multilevel[fifo-1].push(multilevel[fifo].front());
                multilevel[fifo].pop();
            }
        }

        // Access Queues 
        for(int i = 0; i < multilevel.size(); i++){
            // If Queue Is Not Empty, Burst
            if(!multilevel[i].empty()){
                // If Unset, Set Start Time And Last Burst Time
                if(multilevel[i].front().start_time == -1){
                    multilevel[i].front().start_time = time;
                    multilevel[i].front().last_burst = multilevel[i].front().arrival_time;
                }
                // If Active PID Changes 
                if (apid != multilevel[i].front().pid){
                    // Report
                    apid = multilevel[i].front().pid;
                    if(to_file){myfile << "Process " << std::setw(width) << std::right << multilevel[i].front().pid << std::setw(wid) << std::left << " Runs @ " << time << "\n";}
                    else{std::cout << "Process " << std::setw(width) << std::right << multilevel[i].front().pid << std::setw(wid) << std::left << " Runs @ " << time << "\n";}
                    // Update Waiting Time
                    multilevel[i].front().waiting_time += (time - multilevel[i].front().last_burst);
                }
                // Burst
                multilevel[i].front().burst_time--;
                quantum[i]--;
                multilevel[i].front().last_burst = time;

                // If Process Finishes
                if(multilevel[i].front().burst_time == 0){
                    // Report
                    if(to_file){myfile << "Process " << std::setw(width) << std::right << multilevel[i].front().pid << std::setw(wid) << std::left << " Complete @ " << time << "\n";}
                    else{std::cout << "Process " << std::setw(width) << std::right << multilevel[i].front().pid << std::setw(wid) << std::left << " Complete @ " << time << "\n";}
                    // Calculate Statistics
                    multilevel[i].front().end_time = time;
                    multilevel[i].front().turnaround_time = multilevel[i].front().end_time - multilevel[i].front().start_time;
                    avg_turnaround += multilevel[i].front().turnaround_time;
                    avg_waiting += multilevel[i].front().waiting_time;
                    // If Completed From FIFO, Reset Age
                    if(i == fifo){
                        age = 0;
                    }
                    // Complete
                    multilevel[i].pop();
                    complete++;
                    // Reset Quantum Time
                    quantum[i] = time_quant * pow(time_quant_mult, i);
                }
                // If Burst Finishes
                if(quantum[i] == 0){
                    // If Not Final Queue And Burst Time Remains
                    if (i != fifo){
                        // Report
                        if(to_file){myfile << "Process " << std::setw(width) << std::right << multilevel[i].front().pid << std::setw(wid) << std::left << " Demoted @ " << time << "\n";}
                        else{std::cout << "Process " << std::setw(width) << std::right << multilevel[i].front().pid << std::setw(wid) << std::left << " Demoted @ " << time << "\n";}
                        // Demote
                        multilevel[i+1].push(multilevel[i].front());
                        multilevel[i].pop();
                    }
                    // Reset Quantum Time
                    quantum[i] = time_quant * pow(time_quant_mult, i); 
                }
                // Burst Only First Found Process By Exiting Loop Conditions
                i = fifo;
            }
        }
        // Progress
        time++;
    }
        
    // Calculate Final Statistics
    avg_turnaround /= complete;
    avg_waiting /= complete;
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

    // Output Final Statistics
    if(to_file){
        myfile.setf(std::ios::fixed, std::ios::floatfield);
        myfile << "Avg. Wait = " << std::setprecision(5) << avg_waiting << " \t Avg. Turnaround = " << std::setprecision(5) << avg_turnaround << "\n";
        myfile << complete << " Processes Completed In " << time << "\n";
        myfile << format_duration(duration) << std::endl;
        myfile.close();
        std::cout << "Report.txt Generated.\n" << std::endl;
    }
    else{
        std::cout.setf(std::ios::fixed, std::ios::floatfield);
        std::cout << "Avg. Wait = " << avg_waiting << " \t Avg. Turnaround = " << avg_turnaround << "\n";
        std::cout << complete << " Processes Completed In " << time << "\n";
        std::cout << format_duration(duration) << std::endl;
    }

    return 0;
}

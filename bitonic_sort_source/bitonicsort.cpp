#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <mpi.h>
#include <algorithm>

// Globals
double timer_start;
double timer_end;
int process_rank;
int num_processes;
int *array;
int array_size;

/* Define Caliper region names */
const char* mainFunc = "main";
const char* data_init_runtime = "data_init_runtime";
const char* comm = "comm";
const char* comp_large = "comp_large";
const char* comm_large = "comm_large";
const char* comp_small = "comp_small";
const char* comm_small = "comm_small";
const char* correctness_check = "correctness_check";


///////////////////////////////////////////////////
// Comparison Function
///////////////////////////////////////////////////
int ComparisonFunc(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

///////////////////////////////////////////////////
// Compare Low
///////////////////////////////////////////////////
void CompareLow(int j) {
    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_large);  // Start communication region for large data exchange

    int i, min;

    // Send entire array to paired H Process
    int send_counter = 0;
    int *buffer_send = new int[array_size + 1];
    MPI_Send(&array[array_size - 1], 1, MPI_INT, process_rank ^ (1 << j), 0, MPI_COMM_WORLD);

    // Receive new min of sorted numbers
    MPI_Recv(&min, 1, MPI_INT, process_rank ^ (1 << j), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Buffers all values which are greater than min sent from H Process
    for (i = 0; i < array_size; i++) {
        if (array[i] > min) {
            buffer_send[send_counter + 1] = array[i];
            send_counter++;
        } else {
            break;
        }
    }

    buffer_send[0] = send_counter;

    // Send partition to paired H process
    MPI_Send(buffer_send, send_counter, MPI_INT, process_rank ^ (1 << j), 0, MPI_COMM_WORLD);

    // Receive info from paired H process
    int *buffer_receive = new int[array_size + 1];
    MPI_Recv(buffer_receive, array_size, MPI_INT, process_rank ^ (1 << j), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Take received buffer of values from H Process which are smaller than current max
    for (i = 1; i < buffer_receive[0] + 1; i++) {
        if (array[array_size - 1] < buffer_receive[i]) {
            array[array_size - 1] = buffer_receive[i];
        } else {
            break;
        }
    }

    CALI_MARK_END(comm_large);  // End communication region for large data exchange
    CALI_MARK_END(comm);

    CALI_MARK_BEGIN(comp_large);  // Start computation region for large data sorting
    // Sequential Sort
    std::qsort(array, array_size, sizeof(int), ComparisonFunc);
    CALI_MARK_END(comp_large);  // End computation region for large data sorting

    // Free allocated memory
    delete[] buffer_send;
    delete[] buffer_receive;
}

///////////////////////////////////////////////////
// Compare High
///////////////////////////////////////////////////
void CompareHigh(int j) {
    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_large);  // Start communication region for large data exchange

    int i, max;

    // Receive max from L Process's entire array
    int *buffer_receive = new int[array_size + 1];
    MPI_Recv(&max, 1, MPI_INT, process_rank ^ (1 << j), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Send min to L Process of current process's array
    int send_counter = 0;
    int *buffer_send = new int[array_size + 1];
    MPI_Send(&array[0], 1, MPI_INT, process_rank ^ (1 << j), 0, MPI_COMM_WORLD);

    // Buffer a list of values which are smaller than max value
    for (i = 0; i < array_size; i++) {
        if (array[i] < max) {
            buffer_send[send_counter + 1] = array[i];
            send_counter++;
        } else {
            break;
        }
    }

    // Receive blocks greater than min from paired process
    MPI_Recv(buffer_receive, array_size, MPI_INT, process_rank ^ (1 << j), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    int recv_counter = buffer_receive[0];

    // Send partition to paired process
    buffer_send[0] = send_counter;
    MPI_Send(buffer_send, send_counter, MPI_INT, process_rank ^ (1 << j), 0, MPI_COMM_WORLD);

    // Take received buffer of values from L Process which are greater than current min
    for (i = 1; i < recv_counter + 1; i++) {
        if (buffer_receive[i] > array[0]) {
            array[0] = buffer_receive[i];
        } else {
            break;
        }
    }

    CALI_MARK_END(comm_large);  // End communication region for large data exchange
    CALI_MARK_END(comm);

    CALI_MARK_BEGIN(comp_large);  // Start computation region for large data sorting
    // Sequential Sort
    std::qsort(array, array_size, sizeof(int), ComparisonFunc);
    CALI_MARK_END(comp_large);  // End computation region for large data sorting

    // Free allocated memory
    delete[] buffer_send;
    delete[] buffer_receive;
}

///////////////////////////////////////////////////
// Main
///////////////////////////////////////////////////
int main(int argc, char *argv[]) {
    CALI_CXX_MARK_FUNCTION;

    // Create caliper ConfigManager object
    cali::ConfigManager mgr;
    mgr.start();

    CALI_MARK_BEGIN(mainFunc);

    int i, j;

    CALI_MARK_BEGIN(comm);

    // Initialization, get # of processes & this PID/rank
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_processes);
    MPI_Comm_rank(MPI_COMM_WORLD, &process_rank);

    CALI_MARK_END(comm);

    CALI_MARK_BEGIN(data_init_runtime);

    // Initialize Array for Storing Random Numbers
    const char* input_type = "1_perc_perturbed";
    int size = 1 << 16;
    array_size = size / num_processes;
    array = new int[array_size];
    bool random = false;
    bool sorted = false;
    bool reverse = false;
    bool perturbed = true;

    if (random){
	// Generate Random Numbers for Sorting (within each process)
        for (i = 0; i < array_size; i++) {
            array[i] = rand() % (size);
        }
    }else if(sorted){
	// Generate sorted input for sorting (within each process)
	int added = process_rank * array_size;
	for(i = 0; i < array_size; i++){
	    array[i] = i + added;
	}
    }else if(reverse){
	// Generate reverse sorted input for sorting (within each process)
	int added = (num_processes - process_rank) * array_size;
	array[0] = added;
	for (i = 1; i < array_size; i++) {
            array[i] = array[i-1] - 1;
        }
    }else if(perturbed){
	// Generate sorted 1% perturbed input for sorting (within each process)
	int added = process_rank * array_size;
	for(i = 0; i < array_size; i++){
	    array[i] = i + added;
	}
	int temp = array[0];
	array[0] = array[1];
	array[1] = temp;
    }
    std::cout << std::endl;


    srand(time(NULL));

    CALI_MARK_END(data_init_runtime);

    CALI_MARK_BEGIN(comm);
    // Blocks until all processes have finished generating
    MPI_Barrier(MPI_COMM_WORLD);
    CALI_MARK_END(comm);


    // Cube Dimension
    int dimensions = static_cast<int>(std::log2(num_processes));

    // Start Timer before starting first sort operation
    if (process_rank == 0) {
        std::cout << "Number of Processes spawned: " << num_processes << std::endl;
        timer_start = MPI_Wtime();
    }

    CALI_MARK_BEGIN(comp_large);
    // Sequential Sort
    std::qsort(array, array_size, sizeof(int), ComparisonFunc);
    
    // Bitonic Sort follows
    for (i = 0; i < dimensions; i++) {
        for (j = i; j >= 0; j--) {
            if (((process_rank >> (i + 1)) % 2 == 0 && (process_rank >> j) % 2 == 0) ||
                ((process_rank >> (i + 1)) % 2 != 0 && (process_rank >> j) % 2 != 0)) {
                CompareLow(j);
            } else {
                CompareHigh(j);
            }
        }
    }
    CALI_MARK_END(comp_large);

 
    CALI_MARK_BEGIN(comm);
    // Blocks until all processes have finished sorting
    MPI_Barrier(MPI_COMM_WORLD);
    CALI_MARK_END(comm);

    bool works = true;

    CALI_MARK_BEGIN(correctness_check);
    for (i = 1; i < array_size; i++) {
        if(array[i-1] > array[i]) {	
            works = false;
	}
    }
    CALI_MARK_END(correctness_check);

    if(works){
	std::cout << "process " << process_rank << " is sorted.";
    }else{
	std::cout << "process " << process_rank << "is not sorted";
    }
    std::cout << std::endl;


    adiak::init(NULL);
    adiak::launchdate();    // launch date of the job
    adiak::libraries();     // Libraries used
    adiak::cmdline();       // Command line used to launch the job
    adiak::clustername();   // Name of the cluster
    adiak::value("algorithm", "bitonic"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
    adiak::value("programming_model", "mpi"); // e.g. "mpi"
    adiak::value("data_type", "int"); // The datatype of input elements (e.g., double, int, float)
    adiak::value("size_of_data_type", sizeof(int)); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
    adiak::value("input_size", size); // The number of elements in input dataset (1000)
    adiak::value("input_type", input_type); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
    adiak::value("num_procs", num_processes); // The number of processors (MPI ranks)
    adiak::value("scalability", "strong"); // The scalability of your algorithm. choices: ("strong", "weak")
    adiak::value("group_num", 4); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", "online/ai"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten")

    // Free allocated memory
    delete[] array;

    // Done

    // Flush Caliper output before finalizing MPI
    mgr.stop();
    mgr.flush();

    CALI_MARK_BEGIN(comm);
    MPI_Finalize();
    CALI_MARK_END(comm);

    CALI_MARK_END(mainFunc);
    

    return 0;
}


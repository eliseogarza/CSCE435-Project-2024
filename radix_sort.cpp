/******************************************************************************
* FILE: radix_sort.cpp
* DESCRIPTION:
*   This program parallelizes radix sort in an effort to improve
*   computational times and offer a way to improve performance
* AUTHOR: Eliseo Garza
* LAST REVISED: 10/24/2024
******************************************************************************/

#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctime>
#include <random>
#include <algorithm>

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#define MASTER 0 // taskid of root process

// Helper function for finding max value in an array
int findMax(int *arr, int n) {
    int maxVal = arr[0];
    for (int i = 0; i < n; i++) {
        if (arr[i] > maxVal) { maxVal = arr[i]; }
    }

    return maxVal;
}

// Counting sort, stable implementation, goes digit by digit
void countingSort(int *arr, int *count, int n, int exp) {
    int *tempArr =  new int[n];

    // Use this array to store the values of each digit and repurpose to hold the sum of other counts
    int rollingCount[10];

    for (int i = 0; i < n; i++) {
        count[(arr[i] / exp) % 10]++;
    }

    // Copy the counts to the rolling/working count array
    for (int i = 0; i < 10; i++) {
        rollingCount[i] = count[i];
    }

    // Sum the values
    for (int i = 1; i < 10; i++) {
        rollingCount[i] += rollingCount[i - 1];
    }

    // Move input values according to rollingCount
    for (int i = n - 1; i >= 0; i--) {
        int index = (arr[i] / exp) % 10;
        tempArr[rollingCount[index] - 1] = arr[i];
        rollingCount[index]--;
    }

    for (int i = 0; i < n; i++) {
        arr[i] = tempArr[i];
    }

    delete[] tempArr;
}

// Used to directly compare two arrays for use in checking correctness
int compareArrays(int *seqArr, int *parallelArr, int n) {
    for (int i = 0; i < n; i++) {
        if (seqArr[i] != parallelArr[i]) {
            return 0;
        }
    }
    return 1;
}

// Sequential implementation of Radix Sort for comparing end results
void sequentialRadix(int *arr, int n) {
    int maxVal = findMax(arr, n);

    for (int exp = 1; maxVal / exp > 0; exp *= 10) {
        int count[10] = {0};
        countingSort(arr, count, n, exp);
    }
}

// Check the correctness of the parallelized vs sequential version of Radix Sort
int correctnessCheck(int *original, int *sortedArr, int n) {
    int *seqSort = new int[n];
    for (int i = 0; i < n; i++) {
        seqSort[i] = original[i];
    }

    sequentialRadix(seqSort, n);
    int returnVal = compareArrays(seqSort, sortedArr, n);
    delete[] seqSort;

    return returnVal;
}

int main(int argc, char *argv[]) {
    CALI_CXX_MARK_FUNCTION;
    int pow;
    int array_size;
    char array_type;
    std::string input_type;

    if (argc == 3) {
        pow = atoi(argv[1]);
        array_size = 1 << pow; // 2^pow
        array_type = argv[2][0];
    }
    else {
        printf("\n Please provide the power for the array size (ex. 16 for 2^16), the number of process, and type of array ('u' for random/unsorted, 's' for sorted, 'r' for reverse sorted, 'p' for 1%% perturbed) without quotation marks.\n");
        return 0;
    }

    // Create caliper ConfigManager object
    cali::ConfigManager mgr;
    mgr.start();

    int world_rank, world_size;

    // Names for use in caliper regions
    const char* data_init_runtime = "data_init_runtime";
    const char* comm = "comm";
    const char* comm_small = "comm_small";
    const char* comm_large = "comm_large";
    const char* comp = "comp";
    const char* comp_small = "comp_small";
    const char* comp_large = "comp_large";
    const char* correctness_check = "correctness_check";

    // Initializing MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD,&world_rank);
    MPI_Comm_size(MPI_COMM_WORLD,&world_size);

    MPI_Barrier(MPI_COMM_WORLD);

    // Initialize variables for later use
    int *originalArr;
    int *adjustedArr;
    int adjustedSize, maxVal, subinputSize, offset;
    int sizeLimit = 1000000; // FIXME: Change this to adjust largest numbers generated for array
    
    // ************************************

    if (world_rank == MASTER) {
        originalArr = new int[array_size];

        printf("Power: %d\n", pow);
        printf("Array Size: 2^%d , which is %d\n", pow, array_size);

        CALI_MARK_BEGIN(data_init_runtime);

        // Seed random number generator
        srand(static_cast<unsigned int>(time(0)));

        // Generate input array randomly
        if (array_type == 'u') {
            for (int i = 0; i < array_size; i++) {
                originalArr[i] = rand() % sizeLimit;
            }

            input_type = "Random";

            printf("Array Type: Random/Unsorted\n");
        }
        
        // Generate a sorted array
        else if (array_type == 's') {
            // for (int i = 0; i < array_size; i++) {
            //     originalArr[i] = i;
            // }

            for (int i = 0; i < array_size; i++) {
                originalArr[i] = rand() % sizeLimit;
            }

            // Sorts in ascending order
            sequentialRadix(originalArr, array_size);

            input_type = "Sorted";

            printf("Array Type: Sorted\n");
        }
        
        // Generate a reverse sorted array
        else if (array_type == 'r') {
            // for (int i = 0; i < array_size; i++) {
            //     originalArr[i] = array_size - i - 1;
            // }

            for (int i = 0; i < array_size; i++) {
                originalArr[i] = rand() % sizeLimit;
            }

            // Sorts in ascending order (then need to reverse the array)
            sequentialRadix(originalArr, array_size);
            std::reverse(originalArr, originalArr + array_size);

            input_type = "ReverseSorted";

            printf("Array Type: Reverse Sorted\n");
        }
        
        // Generate a 1% perturbed array
        else if (array_type == 'p') {
            // First create sorted array
            for (int i = 0; i < array_size; i++) {
                originalArr[i] = rand() % sizeLimit;
            }

            sequentialRadix(originalArr, array_size);

            // Then swap 1% of elements randomly
            for (int i = 0; i < ceil(array_size / 100.0); i++) {
                std::swap(originalArr[rand() % array_size], originalArr[rand() % array_size]);
            }

            input_type = "1_perc_perturbed";

            printf("Array Type: 1%% perturbed\n");
        }

        // Bad input for array_type, defaulting to random and letting user know
        else {
            printf("Didn't correctly specify type of array, defaulting to random.\n");
            for (int i = 0; i < array_size; i++) {
                originalArr[i] = rand() % sizeLimit;
            }

            input_type = "Random";
        }

        CALI_MARK_END(data_init_runtime);

        printf("Started radix_sort for an array of size %d with %d processes.\n", array_size, world_size);

        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comp_small);

        // Find number of extra zeroes needed for equal processor workloads
        subinputSize = ceil(array_size / ((double) world_size));
        offset = world_size * subinputSize - array_size;

        adjustedSize = subinputSize * world_size;
        adjustedArr = new int[adjustedSize];

        for (int i = 0; i < adjustedSize; i++) {
            if (i < offset) {
                adjustedArr[i] = 0;
            } else {
                adjustedArr[i] = originalArr[i - offset];
            }
        }

        maxVal = findMax(originalArr, array_size);

        CALI_MARK_END(comp_small);
        CALI_MARK_END(comp);
    }

    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_small);

    MPI_Bcast(&maxVal, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
    MPI_Bcast(&array_size, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
    MPI_Bcast(&subinputSize, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
    MPI_Bcast(&adjustedSize, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

    CALI_MARK_END(comm_small);
    CALI_MARK_END(comm);

    // Make sure all processes sync up before continuing
    MPI_Barrier(MPI_COMM_WORLD);

    // Allocate memory for local arrays for each processor
    int *subinput = new int[subinputSize];

    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_small);

    // Scatter the original array into sub-arrays for each processor
    MPI_Scatter(adjustedArr, subinputSize, MPI_INTEGER, subinput, subinputSize, MPI_INTEGER, MASTER, MPI_COMM_WORLD);

    CALI_MARK_END(comm_small);
    CALI_MARK_END(comm);

    int allCounts[10 * world_size];

    // New array for redistribution step
    int *newSubinput = new int[subinputSize];

    int **sendBlocks = new int*[world_size];
    int **recvBlocks = new int*[world_size];

    for (int i = 0; i < world_size; i++) {
        sendBlocks[i] = new int[subinputSize * 2];
        recvBlocks[i] = new int[subinputSize * 2];
    }

    // Counting sort for each digit
    for (int exp = 1; maxVal / exp > 0; exp *= 10) {
        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comp_small);

        // Initialize all values to -1
        for (int i = 0; i < world_size; i++) {
            std::fill(sendBlocks[i], sendBlocks[i] + (subinputSize * 2), -1);
        }

        CALI_MARK_END(comp_small);
        CALI_MARK_END(comp);
        
        // Initialize arrays for future use
        int count[10] = {0};
        int sumCounts[10] = {0};
        int prefixSum[10] = {0};
        int leftSum[10] = {0};

        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comp_small);

        countingSort(subinput, count, subinputSize, exp);

        CALI_MARK_END(comp_small);
        CALI_MARK_END(comp);

        CALI_MARK_BEGIN(comm);
        CALI_MARK_BEGIN(comm_small);

        // Bring in all count arrays and gather them in allCounts
        MPI_Allgather(count, 10, MPI_INTEGER, allCounts, 10, MPI_INTEGER, MPI_COMM_WORLD);

        CALI_MARK_END(comm_small);
        CALI_MARK_END(comm);

        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comp_small);

        // Add up all values into sumCounts array
        for (int i = 0; i < (10 * world_size); i++) {
            int lsd = i % 10;
            int p = i / 10;
            int val = allCounts[i];

            // Store values left of current processor in leftSum
            if (p < world_rank) {
                leftSum[lsd] += val;
            }

            sumCounts[lsd] += val;
            prefixSum[lsd] += val;
        }

        // Create a sum array
        for (int i = 1; i < 10; i++) {
            prefixSum[i] += prefixSum[i - 1];
        }

        MPI_Request request;
        MPI_Status status;
        int lsdSent[10] = {0};

        // Initializing for later use
        int val, lsd, destIndex, destProcess, localDestIndex;
        
        // FIXME: Main issue, changing from regular array to dynamic array based on input size
        // int blockSent[1024] = {0};
        int* blockSent = new int[world_size]();

        CALI_MARK_END(comp_small);
        CALI_MARK_END(comp);

        // Use MPI calls to send and receive information from processes
        for (int i = 0; i < subinputSize; i++) {
            CALI_MARK_BEGIN(comp);
            CALI_MARK_BEGIN(comp_large);

            val = subinput[i];
            lsd = (subinput[i] / exp) % 10;

            // Find the index for the value
            destIndex = prefixSum[lsd] - sumCounts[lsd] + leftSum[lsd] + lsdSent[lsd];

            // Increment the count for elements with lsd and set the value plus index
            lsdSent[lsd]++;
            destProcess = destIndex / subinputSize;
            localDestIndex = destIndex % subinputSize;

            // Sets the values and local index in destination process
            sendBlocks[destProcess][blockSent[destProcess] * 2] = val;
            sendBlocks[destProcess][blockSent[destProcess] * 2 + 1] = localDestIndex;
            blockSent[destProcess]++;

            CALI_MARK_END(comp_large);
            CALI_MARK_END(comp);
        }

        // FIXME: Main issue, changing from regular array to dynamic array based on input size
        // int blockReceive[1024] = {0};
        int* blockReceive = new int[world_size]();

        for (int i = 0; i < world_size; i++) {
            CALI_MARK_BEGIN(comm);
            CALI_MARK_BEGIN(comm_large);

            MPI_Isend(&blockSent[i], 1, MPI_INT, i, 0, MPI_COMM_WORLD, &request);

            // Store the size in a buffer and then write it to blockReceive for the origin processor
            int buffer;
            MPI_Recv(&buffer, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            blockReceive[status.MPI_SOURCE] = buffer;

            CALI_MARK_END(comm_large);
            CALI_MARK_END(comm);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        // Send + receive blocks of a specific size
        for (int i = 0; i < world_size; i++) {
            CALI_MARK_BEGIN(comm);
            CALI_MARK_BEGIN(comm_large);

            MPI_Isend(sendBlocks[i], blockSent[i] * 2, MPI_INT, i, 0, MPI_COMM_WORLD, &request);
            MPI_Recv(recvBlocks[i], blockReceive[i] * 2, MPI_INT, i, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            CALI_MARK_END(comm_large);
            CALI_MARK_END(comm);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comp_small);

        // Build a new sub-array from these sent blocks
        for (int sender = 0; sender < world_size; sender++) {
            for (int i = 0; i < blockReceive[sender]; i++) {
                val = recvBlocks[sender][2 * i];
                localDestIndex = recvBlocks[sender][2 * i + 1];
                newSubinput[localDestIndex] = val;
            }
        }

        for (int i = 0; i < subinputSize; i++) {
          subinput[i] = newSubinput[i];
        }

        CALI_MARK_END(comp_small);
        CALI_MARK_END(comp);

        // Deallocate memory from this iteration here
        delete[] blockSent;
        delete[] blockReceive;
    }

    // Final sorted array sent to root process
    int *finalArr;
    if (world_rank == MASTER) {
        finalArr = new int[adjustedSize];
    }

    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_small);

    // Send all of the sub-arrays into the final array
    MPI_Gather(subinput, subinputSize, MPI_INTEGER, &finalArr[world_rank * subinputSize], subinputSize, MPI_INTEGER, MASTER, MPI_COMM_WORLD);

    CALI_MARK_END(comm_small);
    CALI_MARK_END(comm);

    if (world_rank == MASTER) {
        // Add offset to ignore the added zeroes used to level each processors subarrays
        finalArr += offset;

        CALI_MARK_BEGIN(correctness_check);
        int result = correctnessCheck(originalArr, finalArr, array_size);
        CALI_MARK_END(correctness_check);

        // FIXME: Used this part to check that the final array was sorted
        // ***********************************************************

        // printf("Original Array:\n");

        // for (int i = 0; i < array_size; i++) {
        //     printf("%d ", originalArr[i]);
        // }

        // printf("\nFinal Array:\n");

        // for (int i = 0; i < array_size; i++) {
        //     printf("%d ", finalArr[i]);
        // }

        // ***********************************************************

        if (result) {
            printf("Correctly sorted!\n\n");
        } else {
            printf("Not sorted properly.\n\n");
        }

        // Make sure pointer goes back to original location for de-allocation
        finalArr -= offset;

        delete[] originalArr;
        delete[] adjustedArr;
        delete[] finalArr;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    for (int i = 0; i < world_size; i++) {
        delete[] sendBlocks[i];
        delete[] recvBlocks[i];
    }
    
    delete[] sendBlocks;
    delete[] recvBlocks;
    delete[] subinput;
    delete[] newSubinput;

    adiak::init(NULL);
    adiak::launchdate();    // launch date of the job
    adiak::libraries();     // Libraries used
    adiak::cmdline();       // Command line used to launch the job
    adiak::clustername();   // Name of the cluster
    adiak::value("algorithm", "radix"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
    adiak::value("programming_model", "mpi"); // e.g. "mpi"
    adiak::value("data_type", "int"); // The datatype of input elements (e.g., double, int, float)
    adiak::value("size_of_data_type", "4 bytes"); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
    adiak::value("input_size", array_size); // The number of elements in input dataset (1000)
    adiak::value("input_type", input_type); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
    adiak::value("num_procs", world_size); // The number of processors (MPI ranks)
    adiak::value("scalability", "strong"); // The scalability of your algorithm. choices: ("strong", "weak")
    adiak::value("group_num", "4"); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", "online"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten")

    // Flush Caliper output before finalizing MPI
    mgr.stop();
    mgr.flush();

    MPI_Finalize();
}
/******************************************************************************
* FILE: radix_sort.cpp
* DESCRIPTION:
*   This program parallelizes radix sort in an effort to improve
*   computational times and offer a way to improve performance
* AUTHOR: Eliseo Garza
* LAST REVISED: 10/16/2024
******************************************************************************/

#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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
    int array_size;

    if (argc == 2) {
        array_size = atoi(argv[1]);
    }
    else {
        printf("\n Please provide the size of the array to be sorted");
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

    if (world_rank == MASTER) {
        originalArr = new int[array_size];

        CALI_MARK_BEGIN(data_init_runtime);

        // Generate input array randomly
        for (int i = 0; i < array_size; i++) {
            originalArr[i] = rand() % 1000000;
        }

        CALI_MARK_END(data_init_runtime);

        printf("Started radix_sort for an array of size %d with %d processes.\n", array_size, world_size);

        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comp_small);

        // Find number of extra zeroes needed for equal processor workloads
        subinputSize = ceil(array_size / ((float) world_size));
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

    // Send values needed from MASTER to child processes
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

    // Counting sort for each digit
    for (int exp = 1; maxVal / exp > 0; exp *= 10) {
        // Initialize arrays for future use
        int count[10] = {0};
        int sumCounts[10] = {0};
        int prefixSum[10] = {0};
        int leftSum[10] = {0};

        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comp_large);

        countingSort(subinput, count, subinputSize, exp);

        CALI_MARK_END(comp_large);
        CALI_MARK_END(comp);

        // TODO: 2nd most communciation time spent here
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

        CALI_MARK_END(comp_small);
        CALI_MARK_END(comp);

        MPI_Request request;
        MPI_Status status;
        int lsdSent[10] = {0};

        // Initializing for later use
        int valIndexPair[2];
        int val, lsd, destIndex, destProcess, localDestIndex;

        // Use MPI calls to send and receive information from processes
        for (int i = 0; i < subinputSize; i++) {
            val = subinput[i];
            lsd = (subinput[i] / exp) % 10;

            // Find the index for the value
            destIndex = prefixSum[lsd] - sumCounts[lsd] + leftSum[lsd] + lsdSent[lsd];

            // Increment the count for elements with lsd and set the value plus index
            lsdSent[lsd]++;
            destProcess = destIndex / subinputSize;
            valIndexPair[0] = val;
            valIndexPair[1] = destIndex;

            // TODO: Most communciation time spent here
            CALI_MARK_BEGIN(comm);
            CALI_MARK_BEGIN(comm_large);

            // Send and receive from processes 
            MPI_Isend(&valIndexPair, 2, MPI_INT, destProcess, 0, MPI_COMM_WORLD, &request);
            MPI_Recv(valIndexPair, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            CALI_MARK_END(comm_large);
            CALI_MARK_END(comm);

            // Find the local index of the new value and set it
            localDestIndex = valIndexPair[1] % subinputSize;
            val = valIndexPair[0];
            newSubinput[localDestIndex] = val;
        }

        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comp_small);

        // Transfer the values before continuing
        for (int i = 0; i < subinputSize; i++) {
            subinput[i] = newSubinput[i];
        }

        CALI_MARK_END(comp_small);
        CALI_MARK_END(comp);
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

        if (result) {
            printf("Correctly sorted!\n");
        } else {
            printf("Not sorted properly.\n");
        }

        // Make sure pointer goes back to original location for de-allocation
        finalArr -= offset;

        delete[] originalArr;
        delete[] adjustedArr;
        delete[] finalArr;
    }

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
    adiak::value("input_type", "random"); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
    adiak::value("num_procs", world_size); // The number of processors (MPI ranks)
    adiak::value("scalability", "strong"); // The scalability of your algorithm. choices: ("strong", "weak")
    adiak::value("group_num", "4"); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", "online"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten")

    // Flush Caliper output before finalizing MPI
    mgr.stop();
    mgr.flush();

    MPI_Finalize();
}
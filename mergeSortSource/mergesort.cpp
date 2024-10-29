// C++ program for the implementation of merge sort
#include <iostream>
#include <vector>
#include "mpi.h"
#include <cmath>
#include <caliper/cali.h>
#include <adiak.hpp>
#include <algorithm>
#include <random>
#include <string>

using namespace std;

//merging vectors
void merge(vector<int>& vec, int left, int mid, int right) {
    int i, j, k;
    int n1 = mid - left + 1;
    int n2 = right - mid;

    // Create temporary vectors
    vector<int> leftVec(n1), rightVec(n2);

    // Copy data to temporary vectors
    for (i = 0; i < n1; i++)
        leftVec[i] = vec[left + i];
    for (j = 0; j < n2; j++)
        rightVec[j] = vec[mid + 1 + j];

    // Merge the temporary vectors back into vec[left..right]
    i = 0;
    j = 0;
    k = left;
    while (i < n1 && j < n2) {
        if (leftVec[i] <= rightVec[j]) {
            vec[k] = leftVec[i];
            i++;
        } else {
            vec[k] = rightVec[j];
            j++;
        }
        k++;
    }

    while (i < n1) {
        vec[k] = leftVec[i];
        i++;
        k++;
    }

    while (j < n2) {
        vec[k] = rightVec[j];
        j++;
        k++;
    }
}

// Beggining process of mergeSort
void mergeSort(vector<int>& vec, int left, int right) {
    if (left < right) {
      
        // Calculate the midpoint
        int mid = left + (right - left) / 2;

        // Sort first and second halves
        mergeSort(vec, left, mid);
        mergeSort(vec, mid + 1, right);

        // Merge the sorted halves
        merge(vec, left, mid, right);
    }
}

//Parallel Merge Sort Implementation based upon https://www.christianbaun.de/CGC18/Skript/MPI_TASK_2_Presentation.pdf
void parallelMergeSort(vector<int>& vec, int n, int world_rank, int world_size) {
    CALI_MARK_BEGIN("comm");
    // Divide the array into chunks
    int size = n / world_size;
    vector<int> sub_array(size);

    // Scatter the array to all processes
    CALI_MARK_BEGIN("comm_large");
    MPI_Scatter(vec.data(), size, MPI_INT, sub_array.data(), size, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");
    // Perform merge sort on each process's chunk
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    mergeSort(sub_array, 0, size - 1);
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");
    // Gather the sorted subarrays at the root process
    vector<int> sorted;
    if (world_rank == 0) {
        sorted.resize(n);
    }
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Gather(sub_array.data(), size, MPI_INT, sorted.data(), size, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");
    // Perform final merge at the root process
    if (world_rank == 0) {
        // Handle remaining elements if n is not perfectly divisible
        int remaining_elements = n % world_size;
        if (remaining_elements > 0) {
            vector<int> remaining(remaining_elements);
            MPI_Recv(remaining.data(), remaining_elements, MPI_INT, world_size - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            sorted.insert(sorted.end(), remaining.begin(), remaining.end());
        }
        
        // Merge all sorted arrays
        CALI_MARK_BEGIN("comp");
        CALI_MARK_BEGIN("comp_large");
        mergeSort(sorted, 0, n - 1);
        CALI_MARK_END("comp_large");
        CALI_MARK_END("comp");
        vec = sorted;
    }
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);
    int world_rank;
    int world_size;
    CALI_MARK_BEGIN("main");
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    int array_size = std::atoi(argv[1]);
    string input_type = argv[3];
    vector<int> vec;
    adiak::init(NULL);
    adiak::launchdate();    // launch date of the job
    adiak::libraries();     // Libraries used
    adiak::cmdline();       // Command line used to launch the job
    adiak::clustername();   // Name of the cluster
    adiak::value("algorithm", "merge"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
    adiak::value("programming_model", "mpi"); // e.g. "mpi"
    adiak::value("data_type", "int"); // The datatype of input elements (e.g., double, int, float)
    adiak::value("size_of_data_type", sizeof(int)); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
    adiak::value("input_size", array_size); // The number of elements in input dataset (1000)
    adiak::value("input_type", input_type); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
    adiak::value("num_procs", world_size); // The number of processors (MPI ranks)
    adiak::value("scalability", "strong"); // The scalability of your algorithm. choices: ("strong", "weak")
    adiak::value("group_num", "4"); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", "ai and online: https://www.christianbaun.de/CGC18/Skript/MPI_TASK_2_Presentation.pdf"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").
    CALI_MARK_BEGIN("data_init_runtime");
    if (world_rank == 0) {
        vec.resize(array_size);
        if (input_type == "Sorted") {
        for (int i = 0; i < array_size; ++i) {
            vec[i] = i;
        }
        // cout << "Howdy1" << endl;
    } else if (input_type == "ReverseSorted") {
        for (int i = 0; i < array_size; ++i) {
            vec[i] = array_size - i;
        }
        // cout << "Howdy2" << endl;
    } else if (input_type == "1_perc_perturbed") {
        for (int i = 0; i < array_size; ++i) {
            vec[i] = i;
        }
        // Perturb 1% of the vector
        int num_perturbed = array_size / 100;
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(0, array_size - 1);
        for (int i = 0; i < num_perturbed; ++i) {
            int idx1 = dis(gen);
            int idx2 = dis(gen);
            swap(vec[idx1], vec[idx2]);
        }
        // cout << "Howdy3" << endl;
    } else {
        // Random case
        for (int i = 0; i < array_size; ++i) {
            vec[i] = std::rand() % 10000;
        }
        // cout << "Howdy4" << endl;
    }
    }
    CALI_MARK_END("data_init_runtime");
    parallelMergeSort(vec, array_size, world_rank, world_size);

    CALI_MARK_BEGIN("correctness_check");
    if(world_rank == 0){
        if(is_sorted(vec.begin(), vec.end())){
            // cout << "The vector is sorted!" << endl;
            // cout << world_size << endl;
        }
    }
    CALI_MARK_END("correctness_check");
    MPI_Finalize();
    CALI_MARK_END("main");
    return 0;
}
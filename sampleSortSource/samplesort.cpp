#include <mpi.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>   // for rand, srand
#include <ctime>     // for time
#include <cassert>   // for assert (optional)
#include <cmath>     // for pow

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#define MASTER 0  // Master task identifier

// Helper function to generate random data, now proportional to array size
void data_init_runtime(std::vector<int>& local_data, int size, int rank, int global_size) {
    // Set a max value for random numbers proportional to the global size
    int max_value = global_size / 10;  // Max value is 1/10th of the global array size
    bool random = true;
    bool sorted = false;
    bool reverse = false;
    bool perturbed = false;
    
    if(random){
        srand(time(0) + rank);  // Seed the random number generator with rank
        for (int i = 0; i < size; ++i) {
            local_data.push_back(rand() % max_value);  // Random numbers between 0 and max_value
        }
    }else if(sorted){
        int added = rank * array_size;
        for (int i = 0; i < size; ++i) {
            local_data.push_back(i + added);
        }
    }else if(reverse){
        int added = rank * array_size;
        for (int i = size - 1; i >= 0; --i) {
            local_data.push_back(i + added);
        }
    }else if(perturbed){
        int added = rank * array_size;
        for (int i = 0; i < size; ++i) {
            local_data.push_back(i + added);
        }
        std::swap(local_data[0], local_data[1]);
    }
    
}

// Helper function for correctness check
void correctness_check(const std::vector<int>& data) {
    for (size_t i = 1; i < data.size(); ++i) {
        assert(data[i - 1] <= data[i]);  // Ensure sorted order
    }
    printf("Array sorted");
}

// Choose splitters from the sorted samples
std::vector<int> choose_splitters(const std::vector<int>& sorted_samples, int num_splitters) {
    std::vector<int> splitters;
    int step = sorted_samples.size() / (num_splitters + 1);
    for (int i = 1; i <= num_splitters; ++i) {
        splitters.push_back(sorted_samples[i * step]);
    }
    return splitters;
}

// Sample Sort using MPI
int main(int argc, char* argv[]) {
    // Initialize Caliper and MPI
    CALI_CXX_MARK_FUNCTION;
    MPI_Init(&argc, &argv);

    int numtasks, taskid;
    MPI_Comm_rank(MPI_COMM_WORLD, &taskid);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    
    // Adiak metadata collection
    adiak::init(NULL);
    adiak::launchdate();
    adiak::libraries();
    adiak::cmdline();
    adiak::clustername();

    std::string algorithm = "Sample Sort";
    std::string programming_model = "MPI";
    std::string data_type = "int";
    int size_of_data_type = sizeof(int);
    std::string input_type = "Perturbed";  // Assumed based on random generation
    int num_procs = numtasks;
    std::string scalability = "strong";  // Can be updated depending on your testing
    int group_number = 7;  // Assuming group number 1, update as needed
    std::string implementation_source = "Github: https://github.com/peoro/Parasort/blob/master/doc/Sorting_Algorithms/Sample%20Sort/samplesort_2.c";

    adiak::value("algorithm", algorithm);
    adiak::value("programming_model", programming_model);
    adiak::value("data_type", data_type);
    adiak::value("size_of_data_type", size_of_data_type);
    adiak::value("input_type", input_type);
    adiak::value("num_procs", num_procs);
    adiak::value("scalability", scalability);
    adiak::value("group_num", group_number);
    adiak::value("implementation_source", implementation_source);

    if (numtasks < 2) {
        printf("Need at least two MPI tasks. Exiting...\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        exit(1);
    }

    // Caliper region setup
    cali::ConfigManager mgr;
    mgr.start();

    // Parse array size from command-line arguments
    if (argc != 2) {
        if (taskid == MASTER) {
            std::cerr << "Usage: " << argv[0] << " <array size exponent (e.g., 16 for 2^16)>\n";
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }

    // Get array size (2^exponent) and compute the local size
    int exponent = std::stoi(argv[1]);
    adiak::value("input_size", exponent);
    int global_size = std::pow(2, exponent);  // Size is 2^exponent
    int local_size = global_size / numtasks;

    // Local data initialization
    CALI_MARK_BEGIN("data_init");
    std::vector<int> local_data;
    data_init_runtime(local_data, local_size, taskid, global_size);
    CALI_MARK_END("data_init");

    // Local sort
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("local_sort");
    std::sort(local_data.begin(), local_data.end());
    CALI_MARK_END("local_sort");
    CALI_MARK_END("comp");

    // Select samples
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("select_samples");
    std::vector<int> local_samples;
    int sample_step = local_size / numtasks;
    for (int i = sample_step; i <= local_size; i += sample_step) {
        local_samples.push_back(local_data[i - 1]);
    }
    CALI_MARK_END("select_samples");
    CALI_MARK_END("comp");

    // Gather all samples at master
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("gather_samples");
    std::vector<int> all_samples(numtasks * numtasks);
    MPI_Gather(local_samples.data(), numtasks, MPI_INT, all_samples.data(), numtasks, MPI_INT, MASTER, MPI_COMM_WORLD);
    CALI_MARK_END("gather_samples");
    CALI_MARK_END("comm");

    // Splitters at master
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("choose_splitters");
    std::vector<int> splitters;
    if (taskid == MASTER) {
        std::sort(all_samples.begin(), all_samples.end());
        splitters = choose_splitters(all_samples, numtasks - 1);
    }
    CALI_MARK_END("choose_splitters");
    CALI_MARK_END("comp");

    // Broadcast splitters
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("broadcast_splitters");
    splitters.resize(numtasks - 1);
    MPI_Bcast(splitters.data(), numtasks - 1, MPI_INT, MASTER, MPI_COMM_WORLD);
    CALI_MARK_END("broadcast_splitters");
    CALI_MARK_END("comm");

    // Partition local data based on splitters
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("partition_data");
    std::vector<std::vector<int>> buckets(numtasks);
    for (int i = 0; i < local_size; ++i) {
        int bucket_idx = std::lower_bound(splitters.begin(), splitters.end(), local_data[i]) - splitters.begin();
        buckets[bucket_idx].push_back(local_data[i]);
    }
    CALI_MARK_END("partition_data");
    CALI_MARK_END("comp");

    // Send and receive bucket sizes
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("send_recv_sizes");
    std::vector<int> send_sizes(numtasks);
    std::vector<int> recv_sizes(numtasks);
    std::vector<int> send_displs(numtasks);
    std::vector<int> recv_displs(numtasks);

    for (int i = 0; i < numtasks; ++i) {
        send_sizes[i] = buckets[i].size();
    }

    MPI_Alltoall(send_sizes.data(), 1, MPI_INT, recv_sizes.data(), 1, MPI_INT, MPI_COMM_WORLD);

    send_displs[0] = recv_displs[0] = 0;
    for (int i = 1; i < numtasks; ++i) {
        send_displs[i] = send_displs[i - 1] + send_sizes[i - 1];
        recv_displs[i] = recv_displs[i - 1] + recv_sizes[i - 1];
    }
    CALI_MARK_END("send_recv_sizes");
    CALI_MARK_END("comm");

    // Send and receive buckets
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("send_recv_buckets");
    std::vector<int> send_data;
    for (const auto& bucket : buckets) {
        send_data.insert(send_data.end(), bucket.begin(), bucket.end());
    }
    
    std::vector<int> recv_data(recv_displs[numtasks - 1] + recv_sizes[numtasks - 1]);

    MPI_Alltoallv(send_data.data(), send_sizes.data(), send_displs.data(), MPI_INT,
                  recv_data.data(), recv_sizes.data(), recv_displs.data(), MPI_INT, MPI_COMM_WORLD);
    CALI_MARK_END("send_recv_buckets");
    CALI_MARK_END("comm");

    // Final local sort
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("final_local_sort");
    std::sort(recv_data.begin(), recv_data.end());
    CALI_MARK_END("final_local_sort");
    CALI_MARK_END("comp");

    // Correctness check
    correctness_check(recv_data);

    // Finalize MPI and Caliper
    mgr.flush();
    MPI_Finalize();
    return 0;
}

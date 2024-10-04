# CSCE 435 Group project

## 0. Group number: 4

## 1. Group members:
1. Eliseo Garza
2. Carlos Moreno
3. Daniela Martinez
4. Manuel Estrada

## 2. Project topic (e.g., parallel sorting algorithms)

### 2a. Brief project description (what algorithms will you be comparing and on what architectures)

- Bitonic Sort: Carlos Moreno
- Sample Sort: Daniela Martinez
- Merge Sort: Manuel Estrada
- Radix Sort: Eliseo Garza

### 2b. Pseudocode for each parallel algorithm
- For MPI programs, include MPI calls you will use to coordinate between processes

```
Radix Sort

(Sorting each element starting with LSD -> MSD)

Initialize MPI

Get ranks and determine # of processes and size of array (randomly generated or taken from another file)

Split up input array based on # of processes

Use MPI_Broadcast or MPI_Send to send necessary information to each process if needed

// Compute Radix Sort in each sub-array (via counting sort)
for each digit in element:
    Find digit frequency and store it in a histogram (in each process)
    Send histogram using MPI_Send or MPI_Reduce to main/root process
    Main/root process does calculations on resultant histogram
    Main/root process sends back new histogram to all processes using MPI_Scatter
    Each process adjusts data/number distributions accordingly

Once all processes are finished sorting their sub-arrays, combine/merge them in the main/root process

De-allocate/close MPI communicators and processes
```


### 2c. Evaluation plan - what and how will you measure and compare
- Input sizes, Input types
- Strong scaling (same problem size, increase number of processors/nodes)
- Weak scaling (increase problem size, increase number of processors)
- We will first try our algorithms with input sizes of 2^16, 2^18, 2^20, 2^22, 2^24, 2^26, 2^28.
- We will then for each one try it with a version/type that is sorted, reversed, and sorted with 1% perturbed.
- For each of the different input arrays, we will increase the processors from 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, and increase the number of nodes accordingly.
- We will then increase the number of processors from 2, 4, 8, 16, 32, 64, 128, 256, 512, and 1024 for when the input array size increases.

## 3. Team Communication
For this project, our team will communicate via Messages and Discord.

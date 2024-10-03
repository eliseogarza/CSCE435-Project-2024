# CSCE 435 Group project

## 0. Group number: 4

## 1. Group members:
1. Eliseo Garza
2. Carlos Moreno
3. Daniela Martinez
4. Manuel Estrada

## 2. Project topic (e.g., parallel sorting algorithms)

### 2a. Brief project description (what algorithms will you be comparing and on what architectures)

- Bitonic Sort:
- Sample Sort:
- Merge Sort:
- Radix Sort:

### 2b. Pseudocode for each parallel algorithm
- For MPI programs, include MPI calls you will use to coordinate between processes

### 2c. Evaluation plan - what and how will you measure and compare
- Input sizes, Input types
- Strong scaling (same problem size, increase number of processors/nodes)
- Weak scaling (increase problem size, increase number of processors)
- We will first try our algorithms with input sizes of 2,4,8, 16, 32, 64.
- We will then for each one try it with a version that is sorted, reversed, and sorted with 1% error.
- For each of the different input arrays, we will increase the processors from 2,4,8,16,32,64, and increase the number of nodes accordingly.
- We will then increase the number of processors from 2, 4, 8, 16, 32, and 64 for when the input array size increases.

## 3. Team Communication
For this project, our team will communicate via Messages and Discord.

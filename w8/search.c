#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

int is_prime(int n);
int compare_ints(const void *a, const void *b);

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    int rank, no_of_processes;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &no_of_processes);

    int n;

    // Root reads input
    if (rank == 0)
    {
        if (argc != 2)
        {
            printf("Usage: %s <upper bound>\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        n = atoi(argv[1]);
        if (n <= 0)
        {
            printf("Please enter a positive integer.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    // Broadcast n to all processes
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Each process finds local primes
    int *local_primes = malloc(n * sizeof(int)); // oversize buffer
    int local_count = 0;

    for (int i = 2 + rank; i < n; i += no_of_processes)
    {
        if (is_prime(i))
        {
            local_primes[local_count++] = i;
        }
    }

    // Step 1: gather counts
    int *recv_counts = NULL;
    if (rank == 0)
    {
        recv_counts = malloc(no_of_processes * sizeof(int));
    }
    MPI_Gather(&local_count, 1, MPI_INT,
               recv_counts, 1, MPI_INT,
               0, MPI_COMM_WORLD);

    // Step 2: prepare offsets and gather all primes at root
    int *global_primes = NULL; // will store all primes collected at root
    int *offsets = NULL;       // starting index for each process's data
    int total_prime_count = 0; // total number of primes across all processes

    if (rank == 0)
    {
        // Allocate space for offsets array (size = number of processes)
        offsets = malloc(no_of_processes * sizeof(int));

        // First process's data starts at index 0
        offsets[0] = 0;

        // Each subsequent process starts where the previous one ended
        for (int i = 1; i < no_of_processes; i++)
        {
            offsets[i] = offsets[i - 1] + counts_per_process[i - 1];
        }

        // Compute total number of primes across all processes
        for (int i = 0; i < no_of_processes; i++)
        {
            total_prime_count += counts_per_process[i];
        }

        // Allocate space to hold all primes together
        global_primes = malloc(total_prime_count * sizeof(int));
    }

    // Gather all primes from each process into root process (rank 0)
    MPI_Gatherv(local_primes, local_count, MPI_INT,                  // send buffer
                global_primes, counts_per_process, offsets, MPI_INT, // recv buffer (root only)
                0, MPI_COMM_WORLD);

    // Step 3: root sorts and prints
    if (rank == 0)
    {
        qsort(all_primes, total_primes, sizeof(int), compare_ints);

        printf("Primes less than %d:\n", n);
        for (int i = 0; i < total_primes; i++)
        {
            printf("%d ", all_primes[i]);
        }
        printf("\n");

        free(all_primes);
        free(recv_counts);
        free(displacements);
    }

    free(local_primes);
    MPI_Finalize();
    return 0;
}

// Prime checker
int is_prime(int n)
{
    if (n < 2)
        return 0;
    if (n == 2)
        return 1;
    if (n % 2 == 0)
        return 0;

    int sqrt_n = (int)sqrt(n);
    for (int i = 3; i <= sqrt_n; i += 2)
    {
        if (n % i == 0)
            return 0;
    }
    return 1;
}

// Comparison function for qsort
int compare_ints(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}
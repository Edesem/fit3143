#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

// Prime checker
int is_prime(int n) {
    if (n < 2) return 0;
    if (n == 2) return 1;
    if (n % 2 == 0) return 0;

    int sqrt_n = (int)sqrt(n);
    for (int i = 3; i <= sqrt_n; i += 2) {
        if (n % i == 0) return 0;
    }
    return 1;
}

// Comparison function for qsort
int compare_ints(const void *a, const void *b) {
    int x = *(const int*)a;
    int y = *(const int*)b;
    return (x > y) - (x < y);
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int n;

    // Root reads input
    if (rank == 0) {
        if (argc != 2) {
            printf("Usage: %s <upper bound>\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        n = atoi(argv[1]);
        if (n <= 0) {
            printf("Please enter a positive integer.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    // Broadcast n to all processes
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Each process finds local primes
    int *local_primes = malloc(n * sizeof(int)); // oversize buffer
    int local_count = 0;

    for (int i = 2 + rank; i < n; i += size) {
        if (is_prime(i)) {
            local_primes[local_count++] = i;
        }
    }

    // Step 1: gather counts
    int *recv_counts = NULL;
    if (rank == 0) {
        recv_counts = malloc(size * sizeof(int));
    }
    MPI_Gather(&local_count, 1, MPI_INT,
               recv_counts, 1, MPI_INT,
               0, MPI_COMM_WORLD);

    // Step 2: prepare displacements + gather primes
    int *all_primes = NULL;
    int *displs = NULL;
    int total_primes = 0;

    if (rank == 0) {
        displs = malloc(size * sizeof(int));
        displs[0] = 0;
        for (int i = 1; i < size; i++) {
            displs[i] = displs[i-1] + recv_counts[i-1];
        }

        for (int i = 0; i < size; i++) {
            total_primes += recv_counts[i];
        }

        all_primes = malloc(total_primes * sizeof(int));
    }

    MPI_Gatherv(local_primes, local_count, MPI_INT,
                all_primes, recv_counts, displs, MPI_INT,
                0, MPI_COMM_WORLD);

    // Step 3: root sorts and prints
    if (rank == 0) {
        qsort(all_primes, total_primes, sizeof(int), compare_ints);

        printf("Primes less than %d:\n", n);
        for (int i = 0; i < total_primes; i++) {
            printf("%d ", all_primes[i]);
        }
        printf("\n");

        // (Optional) write to file if n is large
        if (n > 100000) {
            FILE *f = fopen("primes.txt", "w");
            for (int i = 0; i < total_primes; i++) {
                fprintf(f, "%d\n", all_primes[i]);
            }
            fclose(f);
            printf("Primes written to primes.txt\n");
        }

        free(all_primes);
        free(recv_counts);
        free(displs);
    }

    free(local_primes);
    MPI_Finalize();
    return 0;
}

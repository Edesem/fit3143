#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <time.h>

int isPrime(int k) {
    if (k < 2) return 0;
    for (int i = 2; i <= sqrt(k); i++) {
        if (k % i == 0) return 0;
    }
    return 1;
}

int compare(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

double time_diff(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

int main(int argc, char* argv[]) {
    int rank, size;
    struct timespec T_start, T_p1_start, T_p1_end, T_p2_start, T_p2_end, T_sort, T_file;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 2) {
        if (rank == 0) fprintf(stderr, "Usage: %s <n>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    int n = atoi(argv[1]);
    int root_n = (int)sqrt(n);
    int* base_primes = NULL;
    int base_count = 0;

    clock_gettime(CLOCK_MONOTONIC, &T_start);

    // --- Phase 1: Serial on root ---
    clock_gettime(CLOCK_MONOTONIC, &T_p1_start);
    if (rank == 0) {
        base_primes = malloc(sizeof(int) * (root_n + 1));
        for (int i = 2; i <= root_n; i++) {
            if (isPrime(i)) {
                base_primes[base_count++] = i;
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &T_p1_end);

    // --- Broadcast base primes ---
    MPI_Bcast(&base_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) base_primes = malloc(sizeof(int) * base_count);
    MPI_Bcast(base_primes, base_count, MPI_INT, 0, MPI_COMM_WORLD);

    // --- Phase 2 Range Calculation (safe block+remainder) ---
    int total_range = n - root_n - 1;
    int base = total_range / size;
    int remainder = total_range % size;

    int local_start = root_n + 1 + rank * base + (rank < remainder ? rank : remainder);
    int local_end = local_start + base - 1;
    if (rank < remainder) local_end++;

    int local_cap = (n / log(n)) / size + 1000;
    int* local_primes = malloc(sizeof(int) * local_cap);
    int local_count = 0;

    // --- Phase 2 ---
    clock_gettime(CLOCK_MONOTONIC, &T_p2_start);
    for (int k = local_start; k <= local_end; k++) {
        int is_prime = 1;
        int sqrt_k = (int)sqrt(k);
        for (int i = 0; i < base_count && base_primes[i] <= sqrt_k; i++) {
            if (k % base_primes[i] == 0) {
                is_prime = 0;
                break;
            }
        }
        if (is_prime) {
            local_primes[local_count++] = k;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &T_p2_end);

    // --- Gather result sizes ---
    int* recv_counts = NULL;
    int* displs = NULL;
    if (rank == 0) {
        recv_counts = malloc(sizeof(int) * size);
        displs = malloc(sizeof(int) * size);
    }

    MPI_Gather(&local_count, 1, MPI_INT, recv_counts, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int total_phase2_primes = 0;
    int* gathered_primes = NULL;

    if (rank == 0) {
        displs[0] = 0;
        total_phase2_primes += recv_counts[0];
        for (int i = 1; i < size; i++) {
            displs[i] = displs[i - 1] + recv_counts[i - 1];
            total_phase2_primes += recv_counts[i];
        }
        gathered_primes = malloc(sizeof(int) * total_phase2_primes);
    }

    MPI_Gatherv(local_primes, local_count, MPI_INT,
                gathered_primes, recv_counts, displs, MPI_INT,
                0, MPI_COMM_WORLD);

    if (rank == 0) {
        // Merge phase 1 + 2 results
        int total_primes = total_phase2_primes + base_count;
        int* final_primes = malloc(sizeof(int) * total_primes);

        for (int i = 0; i < base_count; i++) {
            final_primes[i] = base_primes[i];
        }
        for (int i = 0; i < total_phase2_primes; i++) {
            final_primes[base_count + i] = gathered_primes[i];
        }

        clock_gettime(CLOCK_MONOTONIC, &T_sort);
        qsort(final_primes, total_primes, sizeof(int), compare);

        clock_gettime(CLOCK_MONOTONIC, &T_file);
        FILE* f = fopen("primes_mpi.txt", "w");
        for (int i = 0; i < total_primes; i++) {
            fprintf(f, "%d\n", final_primes[i]);
        }
        fclose(f);

        printf("Phase 1 (serial):      %.4f sec\n", time_diff(T_p1_start, T_p1_end));
        printf("Phase 2 (parallel):    %.4f sec\n", time_diff(T_p2_start, T_p2_end));
        printf("Sort time:             %.4f sec\n", time_diff(T_p2_end, T_sort));
        printf("File write time:       %.4f sec\n", time_diff(T_sort, T_file));
        printf("Total program time:    %.4f sec\n", time_diff(T_start, T_file));

        free(final_primes);
        free(gathered_primes);
        free(recv_counts);
        free(displs);
    }

    free(base_primes);
    free(local_primes);

    MPI_Finalize();
    return 0;
}

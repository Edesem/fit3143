#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <time.h>

int main(int argc, char* argv[]) {
    int rank, size;
    long N;
    double local_sum = 0.0, global_sum = 0.0;
    double piVal;
    struct timespec start, end;
    double time_taken;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if(rank == 0) {
        printf("Enter the number of intervals N: ");
        fflush(stdout);
        scanf("%ld", &N);
    }

    // Broadcast N to all processes
    MPI_Bcast(&N, 1, MPI_LONG, 0, MPI_COMM_WORLD);

    // Start timing (after N is known)
    if(rank == 0) clock_gettime(CLOCK_MONOTONIC, &start);

    // Compute local sum
    // Divide work among processes, each process gets a range of indices
    long i;
    long chunk_size = N / size;
    long start_i = rank * chunk_size;
    long end_i = (rank == size - 1) ? N : start_i + chunk_size;

    for(i = start_i; i < end_i; i++) {
        double x = (2.0 * i + 1.0) / (2.0 * N);
        local_sum += 4.0 / (1.0 + x * x);
    }

    // Reduce all local sums to global sum at root
    MPI_Reduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    // Compute pi and stop timing
    if(rank == 0) {
        piVal = global_sum / N;
        clock_gettime(CLOCK_MONOTONIC, &end);

        time_taken = (end.tv_sec - start.tv_sec) * 1e9;
        time_taken = (time_taken + (end.tv_nsec - start.tv_nsec)) * 1e-9;
        
        printf("Calculated Pi value (Parallel) = %12.9f\n", piVal);
        printf("Overall time (s): %lf\n", time_taken);
    }

    MPI_Finalize();
    return 0;
}

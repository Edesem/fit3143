#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int main(int argc, char *argv[]) {
    int rank, size, value;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    while (1) {
        // If master/root process
        if (rank == 0) {
            // Root process gets the input
            printf("Enter an integer (negative to quit): ");
            fflush(stdout);
            scanf("%d", &value);

            // Send the value to all other processes
            for (int i = 1; i < size; i++) {
                MPI_Send(&value, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            }
        } else {
            // Other processes receive the value
            MPI_Recv(&value, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        // Everyone prints their result
        printf("Process %d received value: %d\n", rank, value);
        fflush(stdout);

        if (value < 0) {
            break;
        }
    }

    MPI_Finalize();
    return 0;
}

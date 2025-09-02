#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int main(int argc, char *argv[]) {
    int rank, size, value;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    while (1) {
        if (rank == 0) {
            // Root process gets the input
            printf("Enter an integer (negative to quit): ");
            fflush(stdout);
            scanf("%d", &value);
        }

        // Broadcast the value to all processes
        // Like sending a message in a groupchat compared to sending the same text individually to people
        MPI_Bcast(&value, 1, MPI_INT, 0, MPI_COMM_WORLD);

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

int master_io(MPI_Comm master_comm, MPI_Comm comm);
int slave_io(MPI_Comm master_comm, MPI_Comm comm);

int main(int argc, char **argv)
{
    int rank;
    MPI_Comm new_comm;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* (1) Split MPI_COMM_WORLD into master group (color=0) and slaves (color=1) */
    MPI_Comm_split(MPI_COMM_WORLD, (rank == 0 ? 0 : 1), rank, &new_comm);

    if (rank == 0)
        master_io(MPI_COMM_WORLD, new_comm);
    else
        slave_io(MPI_COMM_WORLD, new_comm);

    MPI_Finalize();
    return 0;
}

/* Master process: receives messages from all slaves and prints in order */
int master_io(MPI_Comm master_comm, MPI_Comm comm)
{
    int i, j, size;
    char buf[256];
    MPI_Status status;

    MPI_Comm_size(master_comm, &size);

    for (j = 1; j <= 2; j++) {
        for (i = 1; i < size; i++) {
            MPI_Recv(buf, 256, MPI_CHAR, i, 0, master_comm, &status);
            fputs(buf, stdout);
        }
    }

    return 0;
}

/* Slave processes: send 2 messages to master */
int slave_io(MPI_Comm master_comm, MPI_Comm comm)
{
    char buf[256];
    int rank;

    MPI_Comm_rank(comm, &rank);

    /* (2) Send hello */
    sprintf(buf, "Hello from slave %d\n", rank);
    MPI_Send(buf, strlen(buf) + 1, MPI_CHAR, 0, 0, master_comm);

    /* (3) Send goodbye */
    sprintf(buf, "Goodbye from slave %d\n", rank);
    MPI_Send(buf, strlen(buf) + 1, MPI_CHAR, 0, 0, master_comm);

    return 0;
}

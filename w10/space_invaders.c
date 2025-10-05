#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mpi.h>
#include <unistd.h>  // for sleep()

#define TAG_KILL 1
#define TAG_HIT_PLAYER 2

int main(int argc, char **argv)
{
    int rank, size;
    MPI_Status status;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int tick = 0;
    int player_col = 0;
    bool running = true;

    if (rank == 0)
    {
        // MASTER process
        int remaining_invaders = size - 1;
        while (running)
        {
            int fired = 1; // always fires
            int msg[3] = {tick, player_col, fired};

            // broadcast tick info
            MPI_Bcast(msg, 3, MPI_INT, 0, MPI_COMM_WORLD);

            // probe for events
            int flag = 0;
            while (MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status))
            {
                if (status.MPI_TAG == TAG_KILL)
                {
                    remaining_invaders--;
                    MPI_Recv(NULL, 0, MPI_INT, status.MPI_SOURCE, TAG_KILL, MPI_COMM_WORLD, &status);
                    printf("Master: invader killed. Remaining = %d\n", remaining_invaders);
                }
                else if (status.MPI_TAG == TAG_HIT_PLAYER)
                {
                    MPI_Recv(NULL, 0, MPI_INT, status.MPI_SOURCE, TAG_HIT_PLAYER, MPI_COMM_WORLD, &status);
                    printf("Master: player killed!\n");
                    running = false;
                }
            }

            if (remaining_invaders == 0)
            {
                printf("Master: player wins!\n");
                running = false;
            }

            tick++;
            sleep(1); // 1s per tick
        }

        // send termination broadcast
        int term[3] = {-1, -1, -1};
        MPI_Bcast(term, 3, MPI_INT, 0, MPI_COMM_WORLD);
    }
    else
    {
        // INVADER processes
        int msg[3];
        bool alive = true;
        while (true)
        {
            MPI_Bcast(msg, 3, MPI_INT, 0, MPI_COMM_WORLD);

            // check for termination
            if (msg[0] == -1)
                break;

            int tick = msg[0];
            int player_col = msg[1];
            int fired = msg[2];

            if (alive)
            {
                printf("Invader %d received tick %d, player_col=%d, fired=%d\n",
                       rank, tick, player_col, fired);

                // fake death test
                if (tick == 5 && rank == 1)
                {
                    MPI_Send(NULL, 0, MPI_INT, 0, TAG_KILL, MPI_COMM_WORLD);
                    alive = false;
                }
            }
        }
    }

    MPI_Finalize();
    return 0;
}

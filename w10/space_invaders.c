#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <stdbool.h>
#include <unistd.h> // for sleep()

#define MSG_NOTHING 0
#define MSG_KILL    1
#define MSG_HIT_PLAYER 2

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
            int fired = 1; // player always fires each tick
            int msg[3] = {tick, player_col, fired};

            // broadcast tick info
            MPI_Bcast(msg, 3, MPI_INT, 0, MPI_COMM_WORLD);

            // receive one message from each invader
            for (int i = 1; i < size; i++)
            {
                int event;
                MPI_Recv(&event, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

                if (event == MSG_KILL)
                {
                    remaining_invaders--;
                    printf("Master: invader %d killed. Remaining = %d\n",
                           status.MPI_SOURCE, remaining_invaders);
                }
                else if (event == MSG_HIT_PLAYER)
                {
                    printf("Master: player killed by invader %d!\n", status.MPI_SOURCE);
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

                int event = MSG_NOTHING;

                // fake death test: invader rank 1 dies at tick 5
                if (tick == 5 && rank == 1)
                {
                    event = MSG_KILL;
                    alive = false;
                }

                // TODO: implement firing at player with probability
                // if hit, event = MSG_HIT_PLAYER;

                // send status to master
                MPI_Send(&event, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
            }
            else
            {
                // already dead, still send "nothing" each tick
                int event = MSG_NOTHING;
                MPI_Send(&event, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
            }
        }
    }

    MPI_Finalize();
    return 0;
}

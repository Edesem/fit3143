#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <stdbool.h>
#include <unistd.h>  // for sleep()
#include <time.h>    // for random seeding

#define TAG_KILL 1
#define TAG_HIT_PLAYER 2
#define TAG_OK 3

// Compute mapping: invader ranks -> (row, col)
void get_coords(int rank, int cols, int *row, int *col) {
    int idx = rank - 1; // since rank 0 is master
    *row = idx / cols;
    *col = idx % cols;
}

int main(int argc, char **argv)
{
    int rank, size;
    MPI_Status status;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 3) {
        if (rank == 0)
            fprintf(stderr, "Usage: mpirun -np N ./space_invaders rows cols\n");
        MPI_Finalize();
        return 1;
    }

    int rows = atoi(argv[1]);
    int cols = atoi(argv[2]);
    if (rows * cols != size - 1) {
        if (rank == 0)
            fprintf(stderr, "Error: rows*cols must equal number of invader processes (%d)\n", size - 1);
        MPI_Finalize();
        return 1;
    }

    srand(time(NULL) + rank); // each rank gets different seed

    int tick = 0;
    int player_col = 0;
    bool running = true;

    if (rank == 0)
    {
        // MASTER process
        int remaining_invaders = size - 1;

        while (running)
        {
            int fired = 1; // always fires (player logic)
            int msg[3] = {tick, player_col, fired};

            // broadcast tick info
            MPI_Bcast(msg, 3, MPI_INT, 0, MPI_COMM_WORLD);

            printf("[Master] Tick %d broadcast sent (player_col=%d, fired=%d)\n",
                   tick, player_col, fired);

            // collect heartbeat / event messages from all invaders
            for (int i = 1; i < size; i++) {
                int event;
                MPI_Recv(&event, 1, MPI_INT, i, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

                if (event == TAG_KILL) {
                    remaining_invaders--;
                    printf("[Master] Tick %d: Invader %d killed. Remaining=%d\n",
                           tick, status.MPI_SOURCE, remaining_invaders);
                }
                else if (event == TAG_HIT_PLAYER) {
                    printf("[Master] Tick %d: Player killed by invader %d!\n",
                           tick, status.MPI_SOURCE);
                    running = false;
                }
                else if (event == TAG_OK) {
                    printf("[Master] Tick %d: Invader %d reports OK\n",
                           tick, status.MPI_SOURCE);
                }
            }

            if (remaining_invaders == 0) {
                printf("[Master] Player wins!\n");
                running = false;
            }

            tick++;
            sleep(1); // 1s per tick
        }

        // termination broadcast
        int term[3] = {-1, -1, -1};
        MPI_Bcast(term, 3, MPI_INT, 0, MPI_COMM_WORLD);
    }
    else
    {
        // INVADER processes
        int msg[3];
        bool alive = true;
        int row, col;
        get_coords(rank, cols, &row, &col);

        int scheduled_hit_tick = -1; // when cannonball hits player

        while (alive)
        {
            MPI_Bcast(msg, 3, MPI_INT, 0, MPI_COMM_WORLD);

            // termination check
            if (msg[0] == -1) break;

            int tick = msg[0];
            int player_col = msg[1];
            int fired = msg[2];

            printf("[Invader %d] Tick %d received (player_col=%d, fired=%d)\n",
                   rank, tick, player_col, fired);

            // process pending scheduled shot
            if (scheduled_hit_tick == tick) {
                // send hit event to master
                int event = TAG_HIT_PLAYER;
                MPI_Send(&event, 1, MPI_INT, 0, TAG_HIT_PLAYER, MPI_COMM_WORLD);
                printf("[Invader %d] Hit the player at tick %d!\n", rank, tick);
                scheduled_hit_tick = -1;
            }
            else {
                // decide to fire every 4 ticks
                if (alive && tick > 0 && tick % 4 == 0) {
                    double r = (double) rand() / RAND_MAX;
                    if (r < 0.1) { // 10% chance
                        int travel = 2 + (rows - row - 1); // bottom row travel=2
                        scheduled_hit_tick = tick + travel;
                        printf("[Invader %d] Fires at tick %d (will hit at %d)\n",
                               rank, tick, scheduled_hit_tick);
                    }
                }

                // nothing happened → send heartbeat
                int event = TAG_OK;
                MPI_Send(&event, 1, MPI_INT, 0, TAG_OK, MPI_COMM_WORLD);
            }
        }
    }

    MPI_Finalize();
    return 0;
}

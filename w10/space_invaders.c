#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <stdbool.h>
#include <unistd.h> // for sleep()
#include <time.h>   // for random seeding

#define TAG_KILL 1
#define TAG_HIT_PLAYER 2
#define TAG_OK 3

// Compute mapping: invader ranks -> (row, col)
void get_coords(int rank, int cols, int *row, int *col)
{
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

    if (argc < 3)
    {
        if (rank == 0)
            fprintf(stderr, "Usage: mpirun -np N ./space_invaders rows cols\n");
        MPI_Finalize();
        return 1;
    }

    int rows = atoi(argv[1]);
    int cols = atoi(argv[2]);
    if (rows * cols != size - 1)
    {
        if (rank == 0)
            fprintf(stderr, "Error: rows*cols must equal number of invader processes (%d)\n", size - 1);
        MPI_Finalize();
        return 1;
    }

    srand(time(NULL) + rank); // seed randomness

    int tick = 0;
    int player_col = 0;
    bool running = true;

    if (rank == 0)
    {
        // MASTER process
        int remaining_invaders = size - 1;

        while (running)
        {
            printf("\n=== TICK %d ===\n", tick);

            int fired = 1; // player fires every tick
            int msg[3] = {tick, player_col, fired};

            // broadcast tick info
            MPI_Bcast(msg, 3, MPI_INT, 0, MPI_COMM_WORLD);

            // collect messages from all invaders
            for (int i = 1; i < size; i++)
            {
                int event;
                MPI_Recv(&event, 1, MPI_INT, i, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

                if (event == TAG_KILL)
                {
                    remaining_invaders--;
                    printf("[Master] Invader killed by player. Remaining invaders: %d\n", remaining_invaders);
                }
                else if (event == TAG_HIT_PLAYER)
                {
                    printf("[Master] Player killed by invader %d!\n", i);
                    running = false;
                }
                // TAG_OK means nothing happened
            }

            if (remaining_invaders == 0)
            {
                printf("[Master] Player wins! All invaders defeated.\n");
                running = false;
            }

            // update player position for demo (can cycle left/right)
            player_col = (player_col + 1) % cols;

            tick++;
            sleep(1);
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

        int scheduled_hit_tick = -1;

        while (true)
        {
            MPI_Bcast(msg, 3, MPI_INT, 0, MPI_COMM_WORLD);

            if (msg[0] == -1)
                break; // termination

            tick = msg[0];
            player_col = msg[1];

            printf("[Invader %d] Tick %d | Position: (%d,%d) | Player at col %d\n",
                   rank, tick, row, col, player_col);

            int event_to_send = TAG_OK; // default heartbeat

            if (alive)
            {
                // fake death logic
                if (tick == row + 2 && col == player_col)
                {
                    alive = false;
                    event_to_send = TAG_KILL;
                    printf("[Invader %d] KILLED by player at tick %d!\n", rank, tick);
                }

                // invader shooting
                if (tick > 0 && tick % 4 == 0)
                {
                    double r = (double)rand() / RAND_MAX;
                    if (r < 0.1)
                    {
                        int travel = 2 + (rows - row - 1);
                        scheduled_hit_tick = tick + travel;
                        printf("[Invader %d] Fires at tick %d (will hit player at %d)\n",
                               rank, tick, scheduled_hit_tick);
                    }
                }
            }

            // scheduled shot hits player
            if (scheduled_hit_tick == tick)
            {
                event_to_send = TAG_HIT_PLAYER;
                scheduled_hit_tick = -1;
                printf("[Invader %d] Hit player at tick %d!\n", rank, tick);
            }

            // **always send a message to master**
            MPI_Send(&event_to_send, 1, MPI_INT, 0, event_to_send, MPI_COMM_WORLD);
        }
    }

    MPI_Finalize();
    return 0;
}

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
#define TAG_TERMINATE 4

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

    srand(time(NULL) + rank); // different seed per rank

    int tick = 0;
    int player_col = 0;
    bool running = true;

    if (rank == 0)
    {
        // MASTER process
        int remaining_invaders = size - 1;
        bool invader_alive[size];
        for (int i = 1; i < size; i++) invader_alive[i] = true;

        while (running)
        {
            int fired = 1; // player always fires
            int msg[3] = {tick, player_col, fired};

            printf("[MASTER] === TICK %d ===\n", tick);

            // broadcast tick info
            MPI_Bcast(msg, 3, MPI_INT, 0, MPI_COMM_WORLD);

            // simulate player shot: find bottom-most alive in player_col
            int target = -1;
            int max_row = -1;
            for (int i = 1; i < size; i++) {
                if (!invader_alive[i]) continue;
                int row, col;
                get_coords(i, cols, &row, &col);
                if (col == player_col && row > max_row) {
                    max_row = row;
                    target = i;
                }
            }
            if (target != -1) {
                // send kill message to that invader
                int kill_signal = TAG_KILL;
                MPI_Send(&kill_signal, 1, MPI_INT, target, TAG_KILL, MPI_COMM_WORLD);
                invader_alive[target] = false;
                remaining_invaders--;
                printf("[MASTER] Player shot kills invader %d at tick %d (row=%d,col=%d). Remaining=%d\n",
                       target, tick, max_row, player_col, remaining_invaders);
            }

            // collect one message per invader (OK, HIT_PLAYER, or early death ack)
            for (int i = 1; i < size; i++) {
                if (!invader_alive[i]) continue; // dead invaders stop messaging
                int event;
                MPI_Recv(&event, 1, MPI_INT, i, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

                if (event == TAG_HIT_PLAYER) {
                    printf("[MASTER] Player killed by invader %d at tick %d!\n", i, tick);
                    running = false;
                }
                // TAG_OK = heartbeat, nothing special
            }

            if (remaining_invaders == 0) {
                printf("[MASTER] Player wins!\n");
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

        while (true)
        {
            MPI_Bcast(msg, 3, MPI_INT, 0, MPI_COMM_WORLD);

            // termination check
            if (msg[0] == -1) break;

            int tick = msg[0];
            int player_col = msg[1];
            int fired = msg[2];

            if (!alive) continue; // dead invaders ignore ticks

            printf("[INVADER %d] Tick %d (row=%d,col=%d) player_col=%d fired=%d\n",
                   rank, tick, row, col, player_col, fired);

            // check if master killed me this tick
            int flag;
            MPI_Iprobe(0, TAG_KILL, MPI_COMM_WORLD, &flag, &status);
            if (flag) {
                int dummy;
                MPI_Recv(&dummy, 1, MPI_INT, 0, TAG_KILL, MPI_COMM_WORLD, &status);
                alive = false;
                printf("[INVADER %d] I was killed at tick %d!\n", rank, tick);
                continue;
            }

            // process pending scheduled shot
            if (scheduled_hit_tick == tick) {
                int event = TAG_HIT_PLAYER;
                MPI_Send(&event, 1, MPI_INT, 0, TAG_HIT_PLAYER, MPI_COMM_WORLD);
                printf("[INVADER %d] Hit the player at tick %d!\n", rank, tick);
                scheduled_hit_tick = -1;
            } else {
                // maybe fire every 4 ticks
                if (tick > 0 && tick % 4 == 0) {
                    double r = (double) rand() / RAND_MAX;
                    if (r < 0.1) { // 10% chance
                        int travel = 2 + (rows - row - 1);
                        scheduled_hit_tick = tick + travel;
                        printf("[INVADER %d] Fires at tick %d (will hit at %d)\n",
                               rank, tick, scheduled_hit_tick);
                    }
                }
                // send heartbeat
                int event = TAG_OK;
                MPI_Send(&event, 1, MPI_INT, 0, TAG_OK, MPI_COMM_WORLD);
            }
        }
    }

    MPI_Finalize();
    return 0;
}

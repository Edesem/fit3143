#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <mpi.h>

#define TAG_KILL 1
#define TAG_HIT_PLAYER 2
#define TAG_OK 3

// Compute invader coordinates
void get_coords(int rank, int cols, int *row, int *col) {
    int idx = rank - 1; // rank 0 = master
    *row = idx / cols;
    *col = idx % cols;
}

int main(int argc, char **argv) {
    int rank, size;
    MPI_Status status;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 3) {
        if (rank == 0) fprintf(stderr, "Usage: mpirun -np N ./space_invaders rows cols\n");
        MPI_Finalize();
        return 1;
    }

    int rows = atoi(argv[1]);
    int cols = atoi(argv[2]);

    if (rows * cols != size - 1) {
        if (rank == 0)
            fprintf(stderr, "Error: rows*cols must equal number of invader processes (%d)\n", size-1);
        MPI_Finalize();
        return 1;
    }

    srand(time(NULL) + rank);

    int tick = 0;
    int player_col = 0;
    bool running = true;

    if (rank == 0) {
        // Master process
        bool alive_invaders[size];
        for (int i = 1; i < size; i++) alive_invaders[i] = true;

        while (running) {
            printf("\n=== TICK %d ===\n", tick);

            // Determine bottom-most alive invader per column
            int bottom_most[cols];
            for (int c = 0; c < cols; c++) bottom_most[c] = -1;
            for (int i = 1; i < size; i++) {
                if (!alive_invaders[i]) continue;
                int r, c;
                get_coords(i, cols, &r, &c);
                if (bottom_most[c] == -1 || r > (bottom_most[c]-1)/cols) bottom_most[c] = i;
            }

            // Send tick info + bottom-most info to all invaders
            int msg[2 + cols]; // tick, player_col, bottom-most per column
            msg[0] = tick;
            msg[1] = player_col;
            for (int c = 0; c < cols; c++) msg[2 + c] = bottom_most[c];

            MPI_Bcast(msg, 2 + cols, MPI_INT, 0, MPI_COMM_WORLD);

            // Receive events from invaders
            for (int i = 1; i < size; i++) {
                int event;
                MPI_Recv(&event, 1, MPI_INT, i, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                if (event == TAG_KILL) {
                    alive_invaders[i] = false;
                    printf("[Master] Invader %d killed by player.\n", i);
                } else if (event == TAG_HIT_PLAYER) {
                    printf("[Master] Player killed by invader %d!\n", i);
                    running = false;
                }
            }

            // Check win condition
            bool any_alive = false;
            for (int i = 1; i < size; i++) if (alive_invaders[i]) any_alive = true;
            if (!any_alive) {
                printf("[Master] Player wins! All invaders defeated.\n");
                running = false;
            }

            // Move player for demo (cycle)
            player_col = (player_col + 1) % cols;

            tick++;
            sleep(1);
        }

        // Termination broadcast
        int term[2 + cols];
        term[0] = -1;
        MPI_Bcast(term, 2 + cols, MPI_INT, 0, MPI_COMM_WORLD);

    } else {
        // Invader processes
        int row, col;
        get_coords(rank, cols, &row, &col);

        bool alive = true;
        int scheduled_hit_tick = -1;

        while (true) {
            int msg[2 + cols];
            MPI_Bcast(msg, 2 + cols, MPI_INT, 0, MPI_COMM_WORLD);
            tick = msg[0];
            if (tick == -1) break; // termination
            player_col = msg[1];

            int bottom_most_col = msg[2 + col];

            printf("[Invader %d] Tick %d | Pos: (%d,%d) | Player col: %d | Alive: %d\n",
                   rank, tick, row, col, player_col, alive);

            int event_to_send = TAG_OK;

            if (alive) {
                // Player fire logic
                if (player_col == col && bottom_most_col == rank) {
                    alive = false;
                    event_to_send = TAG_KILL;
                    printf("[Invader %d] KILLED by player at tick %d!\n", rank, tick);
                }

                // Invader fire logic
                if (tick > 0 && tick % 4 == 0 && bottom_most_col == rank) {
                    double r = (double)rand() / RAND_MAX;
                    if (r < 0.1) {
                        int travel = 2 + row; // distance to player
                        scheduled_hit_tick = tick + travel;
                        printf("[Invader %d] Fires at tick %d (will hit player at %d)\n",
                               rank, tick, scheduled_hit_tick);
                    }
                }
            }

            // Cannonball hits player
            if (scheduled_hit_tick == tick) {
                event_to_send = TAG_HIT_PLAYER;
                scheduled_hit_tick = -1;
                printf("[Invader %d] Hit player at tick %d!\n", rank, tick);
            }

            MPI_Send(&event_to_send, 1, MPI_INT, 0, event_to_send, MPI_COMM_WORLD);
        }
    }

    MPI_Finalize();
    return 0;
}

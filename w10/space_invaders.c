#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mpi.h>
#include <time.h>

#define TAG_KILL 1
#define TAG_HIT_PLAYER 2
#define TAG_OK 3

#define MAX_ROWS 100
#define MAX_COLS 100

typedef struct
{
    int col;
    int remaining_ticks;
    bool active;
} Cannonball;

// Compute mapping: invader ranks -> (row, col)
void get_coords(int rank, int cols, int *row, int *col)
{
    int idx = rank - 1; // rank 0 is master
    *row = idx / cols;
    *col = idx % cols;
}

void print_grid(int rows, int cols, bool alive[MAX_ROWS][MAX_COLS], int player_col, Cannonball player_balls[], int pb_count, Cannonball inv_balls[], int ib_count)
{
    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            bool ball_here = false;
            for (int i = 0; i < ib_count; i++)
            {
                if (inv_balls[i].active && inv_balls[i].col == c && inv_balls[i].remaining_ticks == 0 && r == rows - 1)
                {
                    ball_here = true;
                }
            }
            if (!alive[r][c])
                printf(".");
            else if (ball_here)
                printf("*");
            else
                printf("I");
        }
        printf("\n");
    }
    // print player
    for (int c = 0; c < cols; c++)
    {
        if (c == player_col)
            printf("P");
        else
            printf(" ");
    }
    printf("\n");
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

    srand(time(NULL) + rank);

    int tick = 0;
    int player_col = 0;
    bool running = true;

    if (rank == 0)
    {
        // MASTER process
        bool alive[MAX_ROWS][MAX_COLS];
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++)
                alive[r][c] = true;

        Cannonball player_balls[100];
        int pb_count = 0;

        Cannonball inv_balls[100];
        int ib_count = 0;

        int remaining_invaders = rows * cols;

        while (running)
        {
            printf("\n=== TICK %d ===\n", tick);

            // Update player cannonballs
            for (int i = 0; i < pb_count; i++)
            {
                if (!player_balls[i].active)
                    continue;
                player_balls[i].remaining_ticks--;
                if (player_balls[i].remaining_ticks <= 0)
                {
                    int r_hit = -1;
                    for (int r = rows - 1; r >= 0; r--)
                    {
                        if (alive[r][player_balls[i].col])
                        {
                            r_hit = r;
                            alive[r][player_balls[i].col] = false;
                            remaining_invaders--;
                            printf("[Master] Player killed invader at (%d,%d)\n", r, player_balls[i].col);
                            break;
                        }
                    }
                    player_balls[i].active = false;
                }
            }

            // Update invader cannonballs
            for (int i = 0; i < ib_count; i++)
            {
                if (!inv_balls[i].active)
                    continue;
                inv_balls[i].remaining_ticks--;
                if (inv_balls[i].remaining_ticks <= 0)
                {
                    if (inv_balls[i].col == player_col)
                    {
                        printf("[Master] Player hit by invader cannon at col %d!\n", inv_balls[i].col);
                        running = false;
                    }
                    inv_balls[i].active = false;
                }
            }

            // Broadcast tick info to invaders
            int fired = 1; // player fires automatically
            int msg[3] = {tick, player_col, fired};
            MPI_Bcast(msg, 3, MPI_INT, 0, MPI_COMM_WORLD);

            // Receive events from invaders (firing)
            for (int i = 1; i < size; i++)
            {
                int event;
                MPI_Recv(&event, 1, MPI_INT, i, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                if (event > 0)
                { // invader fired
                    inv_balls[ib_count].col = event - 1;
                    inv_balls[ib_count].remaining_ticks = 2 + (rows - 1); // from bottom-most row
                    inv_balls[ib_count].active = true;
                    ib_count++;
                }
            }

            // Visualize grid
            print_grid(rows, cols, alive, player_col, player_balls, pb_count, inv_balls, ib_count);

            if (remaining_invaders == 0)
            {
                printf("[Master] Player wins! All invaders defeated.\n");
                running = false;
            }

            // Move player for demo
            player_col = (player_col + 1) % cols;

            // Fire player cannonball
            int target_row = -1;
            for (int r = rows - 1; r >= 0; r--)
            { // find bottom-most alive invader in column
                if (alive[r][player_col])
                {
                    target_row = r;
                    break;
                }
            }

            if (target_row >= 0)
            { // only fire if a target exists
                player_balls[pb_count].col = player_col;
                player_balls[pb_count].remaining_ticks = 2 + (rows - 1 - target_row); // travel time
                player_balls[pb_count].active = true;
                pb_count++;
            }

            tick++;
            sleep(1);
        }

        // Terminate invaders
        int term[3] = {-1, -1, -1};
        MPI_Bcast(term, 3, MPI_INT, 0, MPI_COMM_WORLD);
    }
    else
    {
        // INVADER process
        int msg[3];
        int row, col;
        get_coords(rank, cols, &row, &col);
        bool alive = true;

        while (true)
        {
            MPI_Bcast(msg, 3, MPI_INT, 0, MPI_COMM_WORLD);
            if (msg[0] == -1)
                break;

            int tick = msg[0];
            int player_col = msg[1];
            int fired = msg[2];

            int event_to_send = 0;

            // Only bottom-most alive invaders can fire
            if (alive && tick > 0 && tick % 4 == 0)
            {
                double r = (double)rand() / RAND_MAX;
                if (r < 0.1)
                {
                    event_to_send = col + 1; // column of firing
                    printf("[Invader %d] Fires at tick %d (column %d)\n", rank, tick, col);
                }
            }

            // Check if killed by player
            if (alive && fired && player_col == col)
            {
                alive = false;
                printf("[Invader %d] KILLED by player at tick %d!\n", rank, tick);
            }

            MPI_Send(&event_to_send, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        }
    }

    MPI_Finalize();
    return 0;
}

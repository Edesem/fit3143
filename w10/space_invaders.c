#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mpi.h>
#include <unistd.h>
#include <time.h>

#define TAG_OK 0
#define TAG_FIRE 1

#define MAX_ROWS 100
#define MAX_COLS 100
#define MAX_BALLS 100

typedef struct
{
    int col;
    int row;
    int target_row;
    int remaining_ticks;
    bool active;
    bool from_player;
} Cannonball;

// Map invader rank -> (row, col)
void get_coords(int rank, int cols, int *row, int *col)
{
    int idx = rank - 1;
    *row = idx / cols;
    *col = idx % cols;
}

// Print grid
void print_grid(int rows, int cols, bool alive[MAX_ROWS][MAX_COLS],
                int player_col, Cannonball player_balls[], int pb_count,
                Cannonball inv_balls[], int ib_count)
{
    char grid[MAX_ROWS][MAX_COLS];

    // Fill grid with invaders
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            grid[r][c] = alive[r][c] ? 'X' : ' ';

    // Place cannonballs
    for (int i = 0; i < pb_count; i++)
    {
        if (player_balls[i].active)
        {
            int r = player_balls[i].row;
            int c = player_balls[i].col;
            if (r >= 0 && r < rows)
                grid[r][c] = '*';
        }
    }
    for (int i = 0; i < ib_count; i++)
    {
        if (inv_balls[i].active)
        {
            int r = inv_balls[i].row;
            int c = inv_balls[i].col;
            if (r >= 0 && r < rows)
                grid[r][c] = 'o';
        }
    }

    // Print rows
    printf("\n--- GAME STATE ---\n");
    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
            printf("%c ", grid[r][c]);
        printf("\n");
    }

    // Print player
    for (int c = 0; c < cols; c++)
        printf("%s", (c == player_col) ? "P " : "  ");
    printf("\n-----------------\n");
}

int main(int argc, char **argv)
{
    int rank, size;
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

    if (rank == 0)
    {
        // MASTER
        bool alive[MAX_ROWS][MAX_COLS];
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++)
                alive[r][c] = true;

        Cannonball player_balls[MAX_BALLS];
        int pb_count = 0;
        Cannonball inv_balls[MAX_BALLS];
        int ib_count = 0;

        int remaining_invaders = rows * cols;
        int player_col = 0;
        int tick = 0;
        bool running = true;

        while (running)
        {
            printf("\n=== TICK %d ===\n", tick);

            // Update player cannonballs
            for (int i = 0; i < pb_count; i++)
            {
                if (!player_balls[i].active)
                    continue;
                player_balls[i].remaining_ticks--;
                player_balls[i].row--;
                if (player_balls[i].remaining_ticks <= 0)
                {
                    alive[player_balls[i].target_row][player_balls[i].col] = false;
                    player_balls[i].active = false;
                    remaining_invaders--;
                    printf("[Master] Player killed invader at (%d,%d)\n",
                           player_balls[i].target_row, player_balls[i].col);
                }
            }

            // Update invader cannonballs
            for (int i = 0; i < ib_count; i++)
            {
                if (!inv_balls[i].active)
                    continue;
                inv_balls[i].remaining_ticks--;
                inv_balls[i].row++;
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

            // Compute bottom-most invader per column
            int bottom_invader[size]; // invader rank allowed to fire per column
            for (int c = 0; c < cols; c++)
            {
                bottom_invader[c] = -1;
                for (int r = rows - 1; r >= 0; r--)
                {
                    if (alive[r][c])
                    {
                        bottom_invader[c] = r * cols + c + 1; // rank of bottom-most invader
                        break;
                    }
                }
            }

            // Broadcast tick info + bottom-most info
            int fired = 1;
            int msg[cols + 3]; // [tick, player_col, fired, bottom-most ranks...]
            msg[0] = tick;
            msg[1] = player_col;
            msg[2] = fired;
            for (int c = 0; c < cols; c++)
                msg[3 + c] = bottom_invader[c];

            MPI_Bcast(msg, cols + 3, MPI_INT, 0, MPI_COMM_WORLD);

            // Receive firing events from invaders
            for (int i = 1; i < size; i++)
            {
                int event;
                MPI_Recv(&event, 1, MPI_INT, i, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                if (event > 0)
                {
                    // Invader fired: create cannonball
                    int col = event - 1;
                    // Find row of that invader
                    int inv_row = -1;
                    for (int r = 0; r < rows; r++)
                        if (alive[r][col])
                            inv_row = r;
                    if (inv_row >= 0)
                    {
                        inv_balls[ib_count].col = col;
                        inv_balls[ib_count].row = inv_row;
                        inv_balls[ib_count].target_row = rows - 1;
                        inv_balls[ib_count].remaining_ticks = 2 + (rows - 1 - inv_row);
                        inv_balls[ib_count].active = true;
                        inv_balls[ib_count].from_player = false;
                        ib_count++;
                    }
                }
            }

            // Fire player cannonball
            int target_row = -1;
            for (int r = rows - 1; r >= 0; r--)
            {
                if (alive[r][player_col])
                {
                    target_row = r;
                    break;
                }
            }
            if (target_row >= 0)
            {
                player_balls[pb_count].col = player_col;
                player_balls[pb_count].row = rows;
                player_balls[pb_count].target_row = target_row;
                player_balls[pb_count].remaining_ticks = 2 + (rows - 1 - target_row);
                player_balls[pb_count].active = true;
                player_balls[pb_count].from_player = true;
                pb_count++;
            }

            print_grid(rows, cols, alive, player_col, player_balls, pb_count, inv_balls, ib_count);

            if (remaining_invaders == 0)
            {
                printf("[Master] Player wins! All invaders defeated.\n");
                running = false;
            }

            player_col = (player_col + 1) % cols;
            tick++;
            sleep(1);
        }

        int term[3] = {-1, -1, -1};
        MPI_Bcast(term, 3, MPI_INT, 0, MPI_COMM_WORLD);
    }
    else
    {
        // INVADER process
        int row, col;
        get_coords(rank, cols, &row, &col);
        bool alive = true;
        int msg[cols + 3];

        while (true)
        {
            MPI_Bcast(msg, cols + 3, MPI_INT, 0, MPI_COMM_WORLD);
            if (msg[0] == -1)
                break;

            int tick = msg[0];
            int player_col = msg[1];
            int fired = msg[2];

            int bottom_rank = msg[3 + col]; // master tells bottom-most rank for this column

            int event_to_send = 0;
            if (alive && rank == bottom_rank && tick > 0 && tick % 4 == 0)
            {
                double r = (double)rand() / RAND_MAX;
                if (r < 0.1)
                    event_to_send = rank; // fire
            }

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

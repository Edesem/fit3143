// space_invaders_fixed.c
// MPI Space Invaders prototype — master + invaders
// Features: bottom-most invader firing, travel-time cannonballs, visuals, compaction
// Compile: mpicc -std=c99 -O2 -o space_invaders_fixed space_invaders_fixed.c
// Run: mpirun -np N ./space_invaders_fixed rows cols

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mpi.h>
#include <unistd.h>
#include <time.h>

#define MAX_ROWS 200
#define MAX_COLS 200
#define MAX_BALLS 1000

typedef struct
{
    int col;
    int row;        // current visual row (0..rows-1). player balls may start at rows and first tick move to rows-1
    int target_row; // target invader row for player balls
    int remaining_ticks;
    bool active;
    bool from_player;
} Cannonball;

// Map invader rank -> (row, col)
void get_coords(int rank, int cols, int *row, int *col)
{
    int idx = rank - 1; // rank 0 is master
    *row = idx / cols;
    *col = idx % cols;
}

// print grid: invaders (X), invader balls (o), player balls (*), player (P)
void print_grid(int rows, int cols, bool alive[MAX_ROWS][MAX_COLS], int player_col,
                Cannonball player_balls[], int pb_count,
                Cannonball inv_balls[], int ib_count)
{
    char grid[MAX_ROWS][MAX_COLS];
    // initialize grid with invader alive/dead
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            grid[r][c] = alive[r][c] ? 'X' : ' ';

    // place invader cannonballs first (so they can be seen beneath player balls if overlap)
    for (int i = 0; i < ib_count; i++)
    {
        if (!inv_balls[i].active)
            continue;
        int r = inv_balls[i].row;
        int c = inv_balls[i].col;
        if (r >= 0 && r < rows && c >= 0 && c < cols)
            grid[r][c] = 'o';
    }

    // place player cannonballs on top
    for (int i = 0; i < pb_count; i++)
    {
        if (!player_balls[i].active)
            continue;
        int r = player_balls[i].row;
        int c = player_balls[i].col;
        if (r >= 0 && r < rows && c >= 0 && c < cols)
            grid[r][c] = '*';
    }

    // print rows top -> bottom (row 0 top)
    printf("\n--- GAME STATE ---\n");
    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            putchar(grid[r][c]);
            putchar(' ');
        }
        putchar('\n');
    }

    // print player row
    for (int c = 0; c < cols; c++)
    {
        if (c == player_col)
            printf("P ");
        else
            printf("  ");
    }
    printf("\n-----------------\n");
}

// compact (remove inactive) cannonballs from array
int compact_player_balls(Cannonball arr[], int count)
{
    int w = 0;
    for (int i = 0; i < count; i++)
    {
        if (arr[i].active)
        {
            if (w != i)
                arr[w] = arr[i];
            w++;
        }
    }
    return w;
}
int compact_inv_balls(Cannonball arr[], int count)
{
    int w = 0;
    for (int i = 0; i < count; i++)
    {
        if (arr[i].active)
        {
            if (w != i)
                arr[w] = arr[i];
            w++;
        }
    }
    return w;
}

int main(int argc, char **argv)
{
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Exit conditions
    if (argc < 3)
    {
        if (rank == 0)
            fprintf(stderr, "Usage: mpirun -np N ./space_invaders_fixed rows cols\n");
        MPI_Finalize();
        return 1;
    }

    int rows = atoi(argv[1]);
    int cols = atoi(argv[2]);
    if (rows <= 0 || cols <= 0)
    {
        if (rank == 0)
            fprintf(stderr, "rows and cols must be positive\n");
        MPI_Finalize();
        return 1;
    }
    if (rows * cols != size - 1)
    {
        if (rank == 0)
            fprintf(stderr, "Error: rows*cols must equal number of invader processes (%d)\n", size - 1);
        MPI_Finalize();
        return 1;
    }
    if (rows > MAX_ROWS || cols > MAX_COLS)
    {
        if (rank == 0)
            fprintf(stderr, "Too large rows or cols (MAX_ROWS=%d MAX_COLS=%d)\n", MAX_ROWS, MAX_COLS);
        MPI_Finalize();
        return 1;
    }

    // Seed for random
    srand((unsigned)time(NULL) + rank * 7919);

    int msg_len = 3 + cols; // [tick, player_col, fired, bottom_rank_for_each_col...]

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

            // --- update player cannonballs: move up and decrement ticks ---
            for (int i = 0; i < pb_count; i++)
            {
                if (!player_balls[i].active)
                    continue;
                player_balls[i].remaining_ticks--;
                player_balls[i].row--; // move up visually
                if (player_balls[i].remaining_ticks <= 0)
                {
                    /// HD TASK A
                    int tr = player_balls[i].target_row;
                    int tc = player_balls[i].col;
                    double p = (double)rand() / RAND_MAX;

                    if (p < 0.2 && tc > 0 && alive[tr][tc - 1])
                    { // 20% left
                        alive[tr][tc - 1] = false;
                    }
                    else if (p < 0.35 && tc < cols - 1 && alive[tr][tc + 1])
                    { // 15% right
                        alive[tr][tc + 1] = false;
                    }
                    else if (p < 0.55)
                    {
                        // shield blocks, do nothing
                    }
                    else if (alive[tr][tc])
                    { // 45% default
                        alive[tr][tc] = false;
                    }

                    // kill bottom-most alive invader in that column at or above target_row
                    // (we assume target_row was computed when fired)
                    if (tr >= 0 && tr < rows && tc >= 0 && tc < cols && alive[tr][tc])
                    {
                        alive[tr][tc] = false;
                        remaining_invaders--;
                        printf("[Master] Player killed invader at (%d,%d)\n", tr, tc);
                    }
                    else
                    {
                        // If target already dead, try to find current bottom-most at that column
                        for (int r = rows - 1; r >= 0; r--)
                        {
                            if (alive[r][tc])
                            {
                                alive[r][tc] = false;
                                remaining_invaders--;
                                printf("[Master] Player killed invader at (%d,%d) (fallback)\n", r, tc);
                                break;
                            }
                        }
                    }
                    player_balls[i].active = false;
                }
            }

            // compact player balls to free slots if many inactive
            if (pb_count > 0 && pb_count > MAX_BALLS / 4)
                pb_count = compact_player_balls(player_balls, pb_count);

            // --- update invader cannonballs: move down and decrement ticks ---
            for (int i = 0; i < ib_count; i++)
            {
                if (!inv_balls[i].active)
                    continue;
                inv_balls[i].remaining_ticks--;
                inv_balls[i].row++; // move down visually
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
            if (ib_count > 0 && ib_count > MAX_BALLS / 4)
                ib_count = compact_inv_balls(inv_balls, ib_count);

            // --- compute bottom-most invader per column and prepare broadcast message ---
            int *msg = (int *)malloc(sizeof(int) * msg_len);
            if (!msg)
            {
                fprintf(stderr, "malloc failed\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
            msg[0] = tick;
            msg[1] = player_col;
            msg[2] = 1; // fired flag (player auto-fires)
            for (int c = 0; c < cols; c++)
            {
                int bottom_rank = -1;
                for (int r = rows - 1; r >= 0; r--)
                {
                    if (alive[r][c])
                    {
                        bottom_rank = r * cols + c + 1; // convert to rank
                        break;
                    }
                }
                msg[3 + c] = bottom_rank;
            }

            // HD task 2
            if (tick % 5 == 0)
            { // every 5 ticks (≈5 seconds with sleep(1))
                for (int r = 0; r < rows; r++)
                {
                    for (int c = 0; c < cols; c++)
                    {
                        if (!alive[r][c])
                        {
                            if (c > 0 && c < cols - 1 && alive[r][c - 1] && alive[r][c + 1])
                            {
                                if ((double)rand() / RAND_MAX < 0.2)
                                {
                                    alive[r][c] = true;
                                    remaining_invaders++;
                                    printf("[Master] Invader reborn at (%d,%d)\n", r, c);
                                }
                            }
                        }
                    }
                }
            }

            // broadcast tick + bottom-most info
            MPI_Bcast(msg, msg_len, MPI_INT, 0, MPI_COMM_WORLD);

            // --- receive events from each invader (one int per invader) ---
            for (int src = 1; src < size; src++)
            {
                int event;
                MPI_Recv(&event, 1, MPI_INT, src, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                if (event > 0)
                {
                    int col = event - 1;
                    // find bottom-most alive invader row in that column (should match master computation)
                    int inv_row = -1;
                    for (int r = rows - 1; r >= 0; r--)
                    {
                        if (alive[r][col])
                        {
                            inv_row = r;
                            break;
                        }
                    }

                    if (inv_row >= 0)
                    {
                        if (ib_count < MAX_BALLS)
                        {
                            inv_balls[ib_count].col = col;
                            inv_balls[ib_count].row = inv_row;
                            inv_balls[ib_count].target_row = rows - 1;
                            inv_balls[ib_count].remaining_ticks = 2 + (rows - 1 - inv_row);
                            inv_balls[ib_count].active = true;
                            inv_balls[ib_count].from_player = false;
                            ib_count++;
                        }
                        else
                        {
                            // drop shot if array full
                        }
                    }

                    else
                    {
                        // no alive invader in column (shouldn't happen if master allowed firing)
                    }
                }
                // event == 0 -> heartbeat/nothing
            }

            // --- Fire player cannonball (create new player cannonball targeted at bottom-most invader in player's column) ---
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
                if (pb_count < MAX_BALLS)
                {
                    player_balls[pb_count].col = player_col;
                    player_balls[pb_count].row = rows; // starts visually below invader area; will move to rows-1 on first update
                    player_balls[pb_count].target_row = target_row;
                    player_balls[pb_count].remaining_ticks = 2 + (rows - 1 - target_row);
                    player_balls[pb_count].active = true;
                    player_balls[pb_count].from_player = true;
                    pb_count++;
                }
                else
                {
                    // drop shot if array full
                }
            }

            // print grid and status
            print_grid(rows, cols, alive, player_col, player_balls, pb_count, inv_balls, ib_count);
            printf("[Master] remaining invaders: %d, active player balls: %d, invader balls: %d\n",
                   remaining_invaders, pb_count, ib_count);

            // check win condition
            if (remaining_invaders <= 0)
            {
                printf("[Master] Player wins! All invaders defeated.\n");
                running = false;
            }

            // move player, left, right, stay
            int move = rand() % 3 - 1; // -1, 0, or +1
            int new_col = player_col + move;
            if (new_col >= 0 && new_col < cols)
            {
                player_col = new_col;
            }
            tick++;
            free(msg);
            sleep(1);
        } // end while running

        // send termination message with same length
        int *term = (int *)malloc(sizeof(int) * msg_len);
        for (int i = 0; i < msg_len; i++)
            term[i] = -1;
        MPI_Bcast(term, msg_len, MPI_INT, 0, MPI_COMM_WORLD);
        free(term);
    }
    else
    {
        // INVADER
        int row, col;
        get_coords(rank, cols, &row, &col);
        bool alive_local = true;

        int msg_len_local = msg_len;
        int *msg = (int *)malloc(sizeof(int) * msg_len_local);
        if (!msg)
        {
            fprintf(stderr, "malloc failed invader\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        while (1)
        {
            MPI_Bcast(msg, msg_len_local, MPI_INT, 0, MPI_COMM_WORLD);

            if (msg[0] == -1)
                break; // termination signaled

            int tick = msg[0];
            int player_col = msg[1];
            int fired = msg[2];
            int bottom_rank_for_my_col = msg[3 + col]; // rank of bottom-most invader for this column, or -1

            int event_to_send = 0; // 0 = no-fire
            if (alive_local && bottom_rank_for_my_col == rank && tick > 0 && (tick % 4) == 0)
            {
                double r = (double)rand() / (double)RAND_MAX;
                if (r < 0.2)
                {
                    // send column+1 as event
                    event_to_send = col + 1;
                    printf("[Invader %d] Fires at tick %d (col %d)\n", rank, tick, col);
                }
            }

            // check if killed by player's shot (master sends fired flag; invader doesn't know who targeted them,
            // but master will mark alive false at impact time; we allow invader to detect being killed only via
            // observing the broadcast bottom_rank become -1 or not equal? To keep things simple, invaders
            // trust the master: they will set alive_local=false when they receive a bottom_rank that no longer
            // identifies them AND their local column no longer has them as alive. However, master will not send
            // per-invader alive flags; instead we rely on master marking invader dead and eventually the invader's
            // own fired attempts will be suppressed because bottom_rank won't equal this rank.)
            //
            // For demonstration simplicity, invaders treat being killed only when player fired and player's column == col
            // AND they are bottom-most at the time of broadcast (master's broadcast pre-kill). The master will later
            // mark them dead and future broadcasts will reflect that. This keeps the protocol simple and avoids extra messages.

            if (alive_local && fired && player_col == col && bottom_rank_for_my_col == rank)
            {
                // The master has indicated player fired this tick and we were the bottom-most when broadcasted,
                // but actual death occurs at master when remaining_ticks reaches zero. To keep invader-side logs in sync,
                // we can optionally log that we were (will be) hit — but we won't flip alive_local here; we'll wait for termination.
                // For clearer invader logs, mark local alive false now (approximation), but master is authoritative.
                alive_local = false;
                printf("[Invader %d] (local) KILLED by player at tick %d (will be removed by master)\n", rank, tick);
            }

            // send heartbeat or fire event (column+1)
            MPI_Send(&event_to_send, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        }

        free(msg);
    }

    MPI_Finalize();
    return 0;
}

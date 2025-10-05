#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#define SHIFT_ROW 0
#define SHIFT_COL 1
#define DISP 1

int is_prime(int n)
{
    if (n <= 1)
        return 0;
    for (int i = 2; i * i <= n; i++)
    {
        if (n % i == 0)
            return 0;
    }
    return 1;
}

int random_prime()
{
    int primes[] = {2,3,5,7,11,13,17,19,23,29,31};
    return primes[rand() % 11];
}


void log_match(int prime, int my_rank, int neighbor_rank)
{
    char filename[32];
    sprintf(filename, "rank_%d.txt", my_rank); // each process writes to its own file
    FILE *fp = fopen(filename, "a");           // append mode
    if (fp != NULL)
    {
        fprintf(fp, "Prime %d matched with neighbor %d\n", prime, neighbor_rank);
        fclose(fp);
    }
}

int main(int argc, char *argv[])
{
    int ndims = 2, size, my_rank, reorder, my_cart_rank, ierr;
    int nrows, ncols;
    int nbr_i_lo, nbr_i_hi;
    int nbr_j_lo, nbr_j_hi;
    MPI_Comm comm2D;
    int dims[ndims], coord[ndims];
    int wrap_around[ndims];
    /* start up initial MPI environment */
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    /* process command line arguments*/
    if (argc == 3)
    {
        nrows = atoi(argv[1]);
        ncols = atoi(argv[2]);
        dims[0] = nrows; /* number of rows */
        dims[1] = ncols; /* number of columns */
        if ((nrows * ncols) != size)
        {
            if (my_rank == 0)
                printf("ERROR: nrows*ncols)=%d * %d = %d != %d\n", nrows, ncols, nrows * ncols, size);

            MPI_Finalize();

            return 0;
        }
    }
    else
    {
        nrows = ncols = (int)sqrt(size);
        dims[0] = dims[1] = 0;
    }

    /************************************************************
     */
    /* create cartesian topology for processes */
    /************************************************************
     */
    MPI_Dims_create(size, ndims, dims);
    if (my_rank == 0)
        printf("Root Rank: %d. Comm Size: %d: Grid Dimension = [%d x %d] \n", my_rank, size, dims[0], dims[1]);

    /* create cartesian mapping */
    wrap_around[0] = wrap_around[1] = 0; /* periodic shift is .false. */
    reorder = 1;
    ierr = 0;
    /* ierr = ... */

    if (ierr != 0)
        printf("ERROR[%d] creating CART\n", ierr);

    /* find my coordinates in the cartesian communicator group */
    ierr = MPI_Cart_create(MPI_COMM_WORLD, ndims, dims, wrap_around, reorder, &comm2D);

    /* use my cartesian coordinates to find my rank in cartesian group*/
    MPI_Comm_rank(comm2D, &my_cart_rank);
    MPI_Cart_coords(comm2D, my_cart_rank, ndims, coord);

    /* get my neighbors; axis is coordinate dimension of shift */

    /* axis=0 ==> shift along the rows: P[my_row-1]: P[me] : P[my_row+1] */
    /* axis=1 ==> shift along the columns P[my_col-1]: P[me] : P[my_col+1] */
    MPI_Cart_shift(comm2D, SHIFT_ROW, DISP, &nbr_i_lo, &nbr_i_hi);
    MPI_Cart_shift(comm2D, SHIFT_COL, DISP, &nbr_j_lo, &nbr_j_hi);

    // loop for 500 iterations
    for (int iter = 0; iter < 500; iter++)
    {
        int my_prime = random_prime();

        int recv_left = -1, recv_right = -1, recv_top = -1, recv_bottom = -1;

        // Exchange with LEFT neighbor
        if (nbr_j_lo != MPI_PROC_NULL)
        {
            MPI_Sendrecv(&my_prime, 1, MPI_INT, nbr_j_lo, 0,
                         &recv_left, 1, MPI_INT, nbr_j_lo, 0,
                         comm2D, MPI_STATUS_IGNORE);

            if (recv_left == my_prime)
            {
                log_match(my_prime, my_rank, nbr_j_lo);
            }
        }

        // Exchange with RIGHT neighbor
        if (nbr_j_hi != MPI_PROC_NULL)
        {
            MPI_Sendrecv(&my_prime, 1, MPI_INT, nbr_j_hi, 0,
                         &recv_right, 1, MPI_INT, nbr_j_hi, 0,
                         comm2D, MPI_STATUS_IGNORE);

            if (recv_right == my_prime)
            {
                log_match(my_prime, my_rank, nbr_j_hi);
            }
        }

        // Exchange with TOP neighbor
        if (nbr_i_lo != MPI_PROC_NULL)
        {
            MPI_Sendrecv(&my_prime, 1, MPI_INT, nbr_i_lo, 0,
                         &recv_top, 1, MPI_INT, nbr_i_lo, 0,
                         comm2D, MPI_STATUS_IGNORE);

            if (recv_top == my_prime)
            {
                log_match(my_prime, my_rank, nbr_i_lo);
            }
        }

        // Exchange with BOTTOM neighbor
        if (nbr_i_hi != MPI_PROC_NULL)
        {
            MPI_Sendrecv(&my_prime, 1, MPI_INT, nbr_i_hi, 0,
                         &recv_bottom, 1, MPI_INT, nbr_i_hi, 0,
                         comm2D, MPI_STATUS_IGNORE);

            if (recv_bottom == my_prime)
            {
                log_match(my_prime, my_rank, nbr_i_hi);
            }
        }
    }

    printf("Global rank: %d. Cart rank: %d. Coord: (%d, %d).Left : %d.Right : % d.Top : % d.Bottom : % d\n ",
           my_rank, my_cart_rank, coord[0], coord[1], nbr_j_lo, nbr_j_hi, nbr_i_lo, nbr_i_hi);
    fflush(stdout);
    MPI_Comm_free(&comm2D);
    MPI_Finalize();
    return 0;
}
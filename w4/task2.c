#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>

typedef struct
{
    int lo;
    int hi;
    int step;
} PrimeArgs;

typedef struct
{
    int *primes;
    int count;
} PrimeResult;

void *find_primes(void *arg);
int is_prime(int n);

int main(int argc, char *argv[])
{
    long num_threads = sysconf(_SC_NPROCESSORS_ONLN);

    if (argc != 2)
    {
        printf("Usage: %s <primes less than n>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]); // convert argument to integer
    if (n <= 0)
    {
        printf("Please enter a positive integer.\n");
        return 1;
    }

    pthread_t thread[num_threads];
    PrimeArgs args[num_threads];

    for (int i = 0; i < num_threads; i++)
    {
        args[i].lo = 2 + i;
        args[i].hi = n;
        args[i].step = num_threads;

        pthread_create(&thread[i], NULL, find_primes, (void *)&args[i]);
    }

    void *void_res[num_threads];
    PrimeResult *res[num_threads];

    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(thread[i], &void_res[i]);
        res[i] = (PrimeResult *)void_res[i];
    }

    // First, compute total number of primes
    int total_count = 0;
    for (int i = 0; i < num_threads; i++)
        total_count += res[i]->count;

    // Allocate one big array
    int *all_primes = malloc(total_count * sizeof(int));

    // Copy thread results into it
    int index = 0;
    for (int i = 0; i < num_threads; i++)
    {
        for (int j = 0; j < res[i]->count; j++)
        {
            all_primes[index++] = res[i]->primes[j];
        }
    }

    int cmpfunc(const void *a, const void *b)
    {
        return (*(int *)a - *(int *)b);
    }

    qsort(all_primes, total_count, sizeof(int), cmpfunc);

    for (int i = 0; i < total_count; i++)
        printf("%d ", all_primes[i]);
    printf("\n");

    for (int i = 0; i < num_threads; i++)
    {
        free(res[i]->primes); // free the array of primes
        free(res[i]);         // free the PrimeResult struct
    }
    free(all_primes);

    return 0;
}

void *find_primes(void *arg)
{
    PrimeArgs *args = (PrimeArgs *)arg;
    // printf("Thread received: lo = %d, hi = %d\n", args->lo, args->hi);

    int lo = args->lo;
    int hi = args->hi;
    int step = args->step;

    int approx_primes = hi / log(hi) * 2;
    int *res = malloc(approx_primes * sizeof(int));

    printf("\n\nSize: %i\n\n", approx_primes);

    int counter = 0;

    // Include 2 if in range
    if (lo <= 2)
    {
        res[counter++] = 2;
        lo = 3;
    }

    if (lo % 2 == 0)
        lo++;

    for (int n = lo; n < hi; n += step)
    {
        if (is_prime(n))
            res[counter++] = n;
    }

    PrimeResult *result = malloc(sizeof(PrimeResult));
    result->primes = res;
    result->count = counter;
    return result;
}

// Simple prime checker
int is_prime(int n)
{
    if (n < 2)
        return 0;
    if (n == 2)
        return 1;
    if (n % 2 == 0)
        return 0;

    int sqrt_n = (int)sqrt(n);
    for (int i = 3; i <= sqrt_n; i += 2)
    {
        if (n % i == 0)
            return 0;
    }
    return 1;
}
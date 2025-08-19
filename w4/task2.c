#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>


typedef struct
{
    int lo;
    int hi;
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

    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores < 1)
    {
        printf("Could not determine number of CPU cores.\n");
    }
    else
    {
        printf("Number of CPU cores: %ld\n", num_cores);
        
    }
    return 0;

    if (argc != 2)
    {
        printf("Usage: %s <primes less than n>\n", argv[0]);
        return 1;
    }

    int input = atoi(argv[1]); // convert argument to integer
    if (input <= 0)
    {
        printf("Please enter a positive integer.\n");
        return 1;
    }

    pthread_t thread1;
    pthread_t thread2;

    int split = input / 2;
    if (split % 2 == 0)
        split++;

    PrimeArgs args1 = {2, split};
    PrimeArgs args2 = {split, input};

    void *void_res1, *void_res2;

    pthread_create(&thread1, NULL, find_primes, (void *)&args1);
    pthread_create(&thread2, NULL, find_primes, (void *)&args2);

    pthread_join(thread1, &void_res1);
    pthread_join(thread2, &void_res2);

    PrimeResult *res1 = (PrimeResult *)void_res1;
    PrimeResult *res2 = (PrimeResult *)void_res2;

    // Now you can safely print only the actual primes:
    for (int i = 0; i < res1->count; i++)
    {
        printf("%d ", res1->primes[i]);
    }
    printf("\n");

    // Now you can safely print only the actual primes:
    for (int i = 0; i < res2->count; i++)
    {
        printf("%d ", res2->primes[i]);
    }
    printf("\n");

    free(res1->primes);
    free(res1);
    free(res2->primes);
    free(res2);

    return 0;
}

void *find_primes(void *arg)
{
    PrimeArgs *args = (PrimeArgs *)arg;
    // printf("Thread received: lo = %d, hi = %d\n", args->lo, args->hi);

    int lo = args->lo;
    int hi = args->hi;

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

    for (int n = lo; n < hi; n += 2)
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
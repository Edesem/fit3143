#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

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

int main(int argc, char *argv[])
{
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

    int remainder, counter = 0, divider, composite;
    // Main loop
    for (int prime = lo; prime < hi; prime += 2)
    {
        composite = 0;
        divider = 2;

        // This offsets the increments, so that it skips every even number after 2, because 2 is the only even prime number
        if (prime == 4)
        {
            prime--;
        }

        // This loop is used to check if the prime variable is actually a prime number
        do
        {
            remainder = prime % divider;

            if (divider < prime && remainder == 0)
            {
                composite = 1;
            }

            if (divider > sqrt(prime) && remainder != 0)
            {
                remainder = 0;
                divider = prime;
            }

            if (remainder != 0)
            {
                divider++;
            }

        } while ((remainder != 0 || prime != divider) && composite != 1);

        composite = 0;

        if (prime == divider && remainder == 0)
        {
            // printf("%i ", prime);
            res[counter++] = prime;
        }

        divider = 2;
    }

    PrimeResult *result = malloc(sizeof(PrimeResult));
    result->primes = res;
    result->count = counter;
    return result;
}

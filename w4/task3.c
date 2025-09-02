#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

// Function prototypes
int find_primes(int n, int res[]);
int is_prime(int n);

int main(int argc, char *argv[])
{
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

    // Rough estimate for number of primes < n (for array size)
    int approx_primes = (int)(n / log(n) * 2);
    int res[approx_primes];

    int count = find_primes(n, res);

    // Print results
    for (int i = 0; i < count; i++)
    {
        printf("%d ", res[i]);
    }
    printf("\n");

    return 0;
}

// Fill res[] with all primes < n, return count
int find_primes(int n, int res[])
{
    int count = 0;

#pragma omp parallel for
    for (int i = 2; i < n; i++)
    {
        if (is_prime(i))
        {
            res[count++] = i;
        }
    }

    return count;
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <number of primes>\n", argv[0]);
        return 1;
    }

    int input = atoi(argv[1]); // convert argument to integer
    if (input <= 0)
    {
        printf("Please enter a positive integer.\n");
        return 1;
    }

    int remainder;
    // Main loop
    for (int prime = 2, counter = 0, divider = 2, composite = 0; counter < input; prime += 2)
    {
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

        }
        while ((remainder != 0 || prime != divider) && composite != 1);

        composite = 0;

        if (prime == divider && remainder == 0)
        {
            printf("%i ", prime);
            counter++;
        }

        divider = 2;
    }

    printf("\n");
    return 0;
}

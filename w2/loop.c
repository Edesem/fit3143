#include <stdio.h>

int main(void)
{
    printf("5's till 100");
    for (int i = 5; i <= 100; i = i + 5)
    {
        printf("%i\n", i);
    }

    printf("\nEven numbers till 50");
    int x = 0;
    while (x <= 50)
    {
        printf("%i\n", x);
        x = x + 2;
    }
}

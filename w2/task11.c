#include <stdio.h>
void calculate(int *a, int *b);
int main()
{ /* RIGHT CODE */
    int a, b;
    a = 5;
    b = 7;
    printf("From main: a = %d, b = %d\n", a, b);
    calculate(&a, &b);
    printf("Back in main: ");
    printf("a = %d, b = %d\n", a, b);
    return (0);
}
void calculate(int *a, int *b)
{
    int temp;
    temp = *a;
    *a = *b;
    *b = temp;
    printf(" From function exchange: ");
    printf("a = %d, b = %d\n", *a, *b);
}
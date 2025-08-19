#include <stdio.h>
int main()
{
    int c, nc = 0, nl = 0, ws = 0;
    while ((c = getchar()) != EOF)
    {
        nc++;
        if (c == '\n')
            nl++;
        
        if (c == ' ')
            ws++;
    }
    printf("number of words = %d\n", ws + 1);
    return (0);
}
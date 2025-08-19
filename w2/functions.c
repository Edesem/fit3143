#include <stdio.h>

void calcRectangle(int length, int width, int *area, int *perimeter) {
    *area = length * width;
    *perimeter = 2 * (length + width);
}

int main(void) {
    int length = 5, width = 3;
    int area, perimeter;

    calcRectangle(length, width, &area, &perimeter);

    printf("Length: %d, Width: %d\n", length, width);
    printf("Area: %d\n", area);
    printf("Perimeter: %d\n", perimeter);
}
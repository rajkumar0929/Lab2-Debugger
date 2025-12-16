#include <stdio.h>

int add(int a, int b) {
    int c = a + b;
    return c;
}

int multiply(int a, int b) {
    int c = a * b;
    return c;
}

int main() {
    int x = 2;
    int y = 3;

    int sum = add(x, y);
    int product = multiply(sum, y);

    printf("sum=%d, product=%d\n", sum, product);
    return 0;
}

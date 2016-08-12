#include <stdio.h>

int reverse_add(int *x, int *y) {
	int a = *x;
	int b = *y;
	*x = b;
	*y = a;

	return a + b;
}

int main() {
	int a = 1;
	int b = 2;

	return reverse_add(&a, &b);
}
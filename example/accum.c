int accum = 0;

int sum(int x, int y) {
	int t = x + y;
	accum += t;
	return t;
}

int main() {
    int a = 3;
    int b = 5;
    int c = sum(a, b);
    return 0;
}

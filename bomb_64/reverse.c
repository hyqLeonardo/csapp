int edx = 14;
int esi = 0;
int edi = 1;

int result;
result = edx - esi;
result = result / 2;
int ecx = result + rsi;

if (ecx <= edi) {
	result = 0;
	if (ecx == edi) {
		return result;
	}
	else if (ecx < edi) {
		esi = rcx + 1;
		result = 2*func(edx, esi, edi)+1;
		return result;
	}
} else if (ecx > edi) {
	edx = rcx - 1;
	result = 2*func(edx, esi, edi);
	return result;
}

9 15 14 5 6 7
9 f e 5 6 7
y o n e f g
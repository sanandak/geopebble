/* Code: */
#include <unistd.h>
int main() {
    struct {
	int a;
	int b;
	float c;
    } x;
    read(STDIN_FILENO, &x, sizeof(x));
    printf("%d %d %f\n", x.a, x.b, x.c);
}


/* testP2.c ends here */

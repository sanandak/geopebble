/* Code: */
#include <unistd.h>
int main() { 
    struct {
	int a;
	int b;
	float c;
    } x;
    x.a=1;
    x.b=2;
    x.c=3.;
    write(STDOUT_FILENO, &x, sizeof(x));
}




/* testP1.c ends here */

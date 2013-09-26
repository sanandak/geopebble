#include <unistd.h>

int main(int argc, char *argv[]) {
    char a[] = "ABC=\"abc\"";
    char b[] = "ABC=\"xyz\"";

    pid_t childPid;
    
    printf("a=%s\n", a);
    putenv(a);

    switch(childPid = fork()) {
    case -1:
	printf("error\n");
	break;
    case 0: /* child */
	while(1) {
	    printf("child %s\n", getenv("ABC"));
	    sleep(1);
	}
	break;
    default: /* parent continues here */
	printf("parent %s\n", getenv("ABC"));
	sleep(5);
	printf("changing ABC\n");
	putenv(b);
	printf("parent %s\n", getenv("ABC"));
	sleep(5);
	break;
    }
}

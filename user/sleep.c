#include "kernel/types.h"
#include "user/user.h"

int xv6_isdigit(char c) {
    return (c >= '0' && c <= '9');
}


int main(int argc, char *argv[]) {

	if (argc != 2) {
		printf("Incorrect number of arguments!\n");
		exit(1);
	}

	for (int j = 0; argv[1][j] != '\0'; j++) {
        if (!xv6_isdigit((unsigned char) argv[1][j])) {
			printf("Invalid argument types!\n");
			exit(1);
		}
    }

	int ticks = atoi(argv[1])*10;
	int sec_ticks = ticks/10; // 10 ticks per second

	if (ticks > 100000) {
		printf("Value too large.\n");
		exit(1);
	}

	printf("Parsed ticks: %d\n", ticks);

	int remaining_ticks = ticks;

	while (remaining_ticks > 0) {
		if (remaining_ticks == 100) { // Example: kill it after 100 ticks
			printf("Process was killed before sleep completed!\n");
			printf("Unslept ticks: %d\n", remaining_ticks);
			printf("Unslept time (seconds): %d\n", remaining_ticks/10);
			exit(1); 
		}

		remaining_ticks--;
	}

	sleep(ticks);
	printf("Sleep successful!\n");
	printf("Seconds: %d\n", sec_ticks);
	exit(0);

}


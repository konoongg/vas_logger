#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {
    printf("PID: %d\n", getpid());
    printf("Enter N to exec self, F to finish: ");
    char input;
    scanf(" %c", &input);
    if (input == 'N' || input == 'n') {
        // exec self
        execl("/proc/self/exe", argv[0], NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    } else if (input == 'F' || input == 'f') {
        printf("Finishing...\n");
        exit(EXIT_SUCCESS);
    } else {
        printf("Invalid input, exiting.\n");
    }
    return 0;
}
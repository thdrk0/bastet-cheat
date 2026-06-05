#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int pid = atoi(argv[1]);
    int old_score = atoi(argv[2]);
    int new_score = 50000;
    int value;

    unsigned long start, end, addr;
    char path[100], line[300];

    sprintf(path, "/proc/%d/maps", pid);
    FILE *maps = fopen(path, "r");

    while (fgets(line, sizeof(line), maps)) {
        if (strstr(line, "[heap]")) {
            sscanf(line, "%lx-%lx", &start, &end);
            break;
        }
    }

    fclose(maps);

    sprintf(path, "/proc/%d/mem", pid);
    int mem = open(path, O_RDWR);

    for (addr = start; addr < end; addr++) {
        pread(mem, &value, sizeof(int), addr);

        if (value == old_score) {
            pwrite(mem, &new_score, sizeof(int), addr);
            printf("Score modifié à l'adresse %lx\n", addr);
            break;
        }
    }

    close(mem);
    return 0;
}

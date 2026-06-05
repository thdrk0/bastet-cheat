#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX 50000

int main(int argc, char **argv)
{
    int pid = atoi(argv[1]);
    int score, value;
    int new_score = 50000;

    unsigned long candidates[MAX];
    int nb = 0;

    char path[100], line[300], perms[10];
    unsigned long start, end, addr;

    printf("[INFO] PID = %d\n", pid);

    sprintf(path, "/proc/%d/mem", pid);
    int mem = open(path, O_RDWR);

    if (mem < 0) {
        printf("[ERREUR] impossible d'ouvrir %s\n", path);
        printf("[SOLUTION] lance avec : sudo ./cheat %d\n", pid);
        return 1;
    }

    printf("Score actuel NON NUL affiche dans le jeu : ");
    scanf("%d", &score);

    if (score == 0) {
        printf("[STOP] Ne mets pas 0. Gagne d'abord des points.\n");
        return 1;
    }

    sprintf(path, "/proc/%d/maps", pid);
    FILE *maps = fopen(path, "r");

    if (maps == NULL) {
        printf("[ERREUR] impossible d'ouvrir %s\n", path);
        return 1;
    }

    while (fgets(line, sizeof(line), maps)) {
        sscanf(line, "%lx-%lx %s", &start, &end, perms);

        if (perms[0] == 'r' && perms[1] == 'w') {
            printf("[SCAN] %lx - %lx %s\n", start, end, perms);

            for (addr = start; addr < end; addr += 4) {
                if (pread(mem, &value, sizeof(int), addr) == sizeof(int)) {
                    if (value == score) {
                        candidates[nb] = addr;
                        nb++;

                        printf("[CANDIDAT] %lx\n", addr);

                        if (nb >= MAX) {
                            printf("[STOP] Trop de candidats. Score trop commun.\n");
                            printf("[CONSEIL] gagne plus de points puis relance avec un score plus grand.\n");
                            return 1;
                        }
                    }
                }
            }
        }
    }

    fclose(maps);

    printf("[INFO] candidats trouves = %d\n", nb);

    while (nb > 1) {
        printf("\nGagne encore des points dans Bastet.\n");
        printf("Nouveau score affiche : ");
        scanf("%d", &score);

        int new_nb = 0;

        for (int i = 0; i < nb; i++) {
            if (pread(mem, &value, sizeof(int), candidates[i]) == sizeof(int)) {
                if (value == score) {
                    candidates[new_nb] = candidates[i];
                    new_nb++;
                }
            }
        }

        nb = new_nb;

        printf("[INFO] candidats restants = %d\n", nb);

        for (int i = 0; i < nb && i < 10; i++) {
            printf("[RESTE] %lx\n", candidates[i]);
        }

        if (nb == 0) {
            printf("[ECHEC] Aucun candidat. Tu as peut-être saisi le mauvais score.\n");
            return 1;
        }
    }

    printf("[OK] Adresse du score = %lx\n", candidates[0]);

    pwrite(mem, &new_score, sizeof(int), candidates[0]);

    pread(mem, &value, sizeof(int), candidates[0]);

    printf("[OK] Nouvelle valeur lue = %d\n", value);

    close(mem);
    return 0;
}

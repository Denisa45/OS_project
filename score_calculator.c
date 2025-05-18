#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_USERNAME 50
#define MAX_CLUE_TEXT 500


typedef struct {
    int treasure_id;
    char username[MAX_USERNAME];
    double latitude;
    double longitude;
    char clue[MAX_CLUE_TEXT];
    int value;
} Treasure;


typedef struct {
    char username[MAX_USERNAME];
    int total_score;
    int treasures_count;
} UserScore;


int compare_scores(const void* a, const void* b) {
    const UserScore* score_a = (const UserScore*)a;
    const UserScore* score_b = (const UserScore*)b;


    return score_b->total_score - score_a->total_score;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hunt_id>\n", argv[0]);
        return 1;
    }

    const char* hunt_id = argv[1];


    char path[256];
    snprintf(path, sizeof(path), "%s/treasures.dat", hunt_id);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error: Could not open %s\n", path);
        return 1;
    }


    Treasure treasure;
    UserScore users[100];
    int user_count = 0;


    memset(users, 0, sizeof(users));

    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {

        int found = 0;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].username, treasure.username) == 0) {

                users[i].total_score += treasure.value;
                users[i].treasures_count++;
                found = 1;
                break;
            }
        }

        if (!found && user_count < 100) {

            strncpy(users[user_count].username, treasure.username, MAX_USERNAME - 1);
            users[user_count].username[MAX_USERNAME - 1] = '\0';
            users[user_count].total_score = treasure.value;
            users[user_count].treasures_count = 1;
            user_count++;
        }
    }

    close(fd);


    qsort(users, user_count, sizeof(UserScore), compare_scores);


    printf("Hunt: %s - User Scores\n", hunt_id);
    printf("---------------------------\n");

    if (user_count == 0) {
        printf("No users found in this hunt.\n");
    } else {
        printf("%-20s | %-12s | %s\n", "Username", "Total Score", "Treasures");
        printf("----------------------------------------------------\n");

        for (int i = 0; i < user_count; i++) {
            printf("%-20s | %-12d | %d\n",
                users[i].username,
                users[i].total_score,
                users[i].treasures_count);
        }
    }

    return 0;
}

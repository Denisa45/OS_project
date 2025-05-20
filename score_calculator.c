#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

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

void store_Calculator(const char* hunt_id, int pipe_fd) {
    char path[256];
    snprintf(path, sizeof(path), "%s/treasures.dat", hunt_id);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        dprintf(pipe_fd, "Error: Could not open %s\n", path);
        return;
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

    dprintf(pipe_fd, "Hunt: %s - User Scores\n", hunt_id);
    dprintf(pipe_fd, "---------------------------\n");

    if (user_count == 0) {
        dprintf(pipe_fd, "No users found in this hunt.\n");
    } else {
        dprintf(pipe_fd, "%-20s | %-12s | %s\n", "Username", "Total Score", "Treasures");
        dprintf(pipe_fd, "----------------------------------------------------\n");
        for (int i = 0; i < user_count; i++) {
            dprintf(pipe_fd, "%-20s | %-12d | %d\n",
                users[i].username,
                users[i].total_score,
                users[i].treasures_count);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hunt_id>\n", argv[0]);
        return 1;
    }

    int pipe_to_child[2];
    int pipe_to_parent[2];

    if (pipe(pipe_to_child) == -1 || pipe(pipe_to_parent) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // Child process
        close(pipe_to_child[1]); // close write end of input pipe
        close(pipe_to_parent[0]); // close read end of output pipe

        char hunt_id[100];
        read(pipe_to_child[0], hunt_id, sizeof(hunt_id));
        close(pipe_to_child[0]);

        store_Calculator(hunt_id, pipe_to_parent[1]);
        close(pipe_to_parent[1]);
        exit(0);
    } else {
        // Parent process
        close(pipe_to_child[0]); // close read end of input pipe
        close(pipe_to_parent[1]); // close write end of output pipe

        // Send hunt_id to child
        write(pipe_to_child[1], argv[1], strlen(argv[1]) + 1);
        close(pipe_to_child[1]);

        // Read output from child
        char buffer[1024];
        ssize_t n;
        while ((n = read(pipe_to_parent[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[n] = '\0';
            printf("%s", buffer);
        }
        close(pipe_to_parent[0]);
    }

    return 0;
}

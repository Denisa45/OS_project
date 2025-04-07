#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>

#define RECORD_SIZE 376
#define USERNAME_SIZE 100
#define CLUE_SIZE 255

typedef struct {
    uint32_t id;
    char username[USERNAME_SIZE];
    double latitude;
    double longitude;
    char clue[CLUE_SIZE];
    uint32_t value;
} Treasure;

void log_operation(const char *hunt_path, const char *operation) {
    char log_path[256];
    snprintf(log_path, sizeof(log_path), "%s/logged_hunt", hunt_path);
    FILE *log_file = fopen(log_path, "a");
    if (log_file) {
        time_t now = time(NULL);
        fprintf(log_file, "%s: %s\n", operation, ctime(&now));
        fclose(log_file);
    }
}

void add_treasure(const char *hunt_id) {
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "%s", hunt_id);
    mkdir(dir_path, 0777);

    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/treasures.dat", dir_path);

    FILE *file = fopen(file_path, "ab");
    if (!file) { perror("fopen"); return; }

    Treasure t;
    printf("Enter Treasure ID: "); scanf("%u", &t.id);
    printf("Enter Username: "); scanf("%s", t.username);
    printf("Enter Latitude: "); scanf("%lf", &t.latitude);
    printf("Enter Longitude: "); scanf("%lf", &t.longitude);
    printf("Enter Clue: "); getchar(); fgets(t.clue, CLUE_SIZE, stdin);
    printf("Enter Value: "); scanf("%u", &t.value);

    fwrite(&t, sizeof(Treasure), 1, file);
    fclose(file);

    log_operation(dir_path, "Added treasure");
}

void list_treasures(const char *hunt_id) {
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "%s/treasures.dat", hunt_id);

    struct stat st;
    if (stat(dir_path, &st) != 0) {
        perror("stat");
        return;
    }

    printf("Hunt: %s\nSize: %ld bytes\nLast Modified: %s\n", hunt_id, st.st_size, ctime(&st.st_mtime));

    FILE *file = fopen(dir_path, "rb");
    if (!file) { perror("fopen"); return; }

    Treasure t;
    while (fread(&t, sizeof(Treasure), 1, file)) {
        printf("ID: %u, User: %s, Location: (%lf, %lf), Value: %u\nClue: %s\n",
               t.id, t.username, t.latitude, t.longitude, t.value, t.clue);
    }
    fclose(file);
    log_operation(hunt_id, "Listed treasures");
}

void view_treasure(const char *hunt_id, uint32_t id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/treasures.dat", hunt_id);

    FILE *file = fopen(file_path, "rb");
    if (!file) { perror("fopen"); return; }

    Treasure t;
    while (fread(&t, sizeof(Treasure), 1, file)) {
        if (t.id == id) {
            printf("ID: %u, User: %s, Location: (%lf, %lf), Value: %u\nClue: %s\n",
                   t.id, t.username, t.latitude, t.longitude, t.value, t.clue);
            log_operation(hunt_id, "Viewed treasure");
            fclose(file);
            return;
        }
    }
    printf("Treasure not found.\n");
    fclose(file);
}

void remove_treasure(const char *hunt_id, uint32_t id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/treasures.dat", hunt_id);

    FILE *file = fopen(file_path, "rb");
    if (!file) { perror("fopen"); return; }

    FILE *temp = fopen("temp.dat", "wb");
    if (!temp) { perror("fopen temp"); fclose(file); return; }

    Treasure t;
    int found = 0;
    while (fread(&t, sizeof(Treasure), 1, file)) {
        if (t.id != id) {
            fwrite(&t, sizeof(Treasure), 1, temp);
        } else {
            found = 1;
        }
    }
    fclose(file);
    fclose(temp);

    if (found) {
        remove(file_path);
        rename("temp.dat", file_path);
        log_operation(hunt_id, "Removed treasure");
    } else {
        printf("Treasure not found.\n");
        remove("temp.dat");
    }
}

void remove_hunt(const char *hunt_id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/treasures.dat", hunt_id);
    remove(file_path);

    snprintf(file_path, sizeof(file_path), "%s/logged_hunt", hunt_id);
    remove(file_path);

    rmdir(hunt_id);
    printf("Hunt %s removed.\n", hunt_id);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <command> <hunt_id> [<id>]\n", argv[0]);
        return 1;
    }

    const char *command = argv[1];
    const char *hunt_id = argv[2];

    if (strcmp(command, "add") == 0) {
        add_treasure(hunt_id);
    } else if (strcmp(command, "list") == 0) {
        list_treasures(hunt_id);
    } else if (strcmp(command, "view") == 0 && argc == 4) {
        view_treasure(hunt_id, (uint32_t)atoi(argv[3]));
    } else if (strcmp(command, "remove_treasure") == 0 && argc == 4) {
        remove_treasure(hunt_id, (uint32_t)atoi(argv[3]));
    } else if (strcmp(command, "remove_hunt") == 0) {
        remove_hunt(hunt_id);
    } else {
        printf("Invalid command or arguments.\n");
        return 1;
    }
    return 0;
}

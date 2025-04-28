#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

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

    int log_fd = open(log_path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (log_fd == -1) {
        perror("open log file");
        return;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char buffer[512];
    strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S] ", tm_info);
    strncat(buffer, operation, sizeof(buffer) - strlen(buffer) - 1);
    strncat(buffer, "\n", sizeof(buffer) - strlen(buffer) - 1);

    write(log_fd, buffer, strlen(buffer));
    close(log_fd);
}

void create_symlink_for_log(const char *hunt_id) {
    char target[256], link_name[256];
    snprintf(target, sizeof(target), "%s/logged_hunt", hunt_id);
    snprintf(link_name, sizeof(link_name), "logged_hunt-%s", hunt_id);
    symlink(target, link_name);
}

void add_treasure(const char *hunt_id) {
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "%s", hunt_id);

    if (mkdir(dir_path, 0755) == -1) {
        if (errno != EEXIST) {
            perror("mkdir");
            return;
        }
    }

    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/treasures", dir_path);

    int fd = open(file_path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1) {
        perror("open treasures");
        return;
    }

    Treasure t;
    printf("Enter Treasure ID: "); scanf("%u", &t.id);
    printf("Enter Username: "); scanf("%s", t.username);
    printf("Enter Latitude: "); scanf("%lf", &t.latitude);
    printf("Enter Longitude: "); scanf("%lf", &t.longitude);
    getchar(); // consume newline
    printf("Enter Clue: "); fgets(t.clue, CLUE_SIZE, stdin);
    t.clue[strcspn(t.clue, "\n")] = '\0'; // remove newline
    printf("Enter Value: "); scanf("%u", &t.value);

    write(fd, &t, sizeof(Treasure));
    close(fd);

    log_operation(dir_path, "ADD treasure");
    create_symlink_for_log(hunt_id);
}

void list_treasures(const char *hunt_id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/treasures", hunt_id);

    struct stat st;
    if (stat(file_path, &st) != 0) {
        perror("stat");
        return;
    }

    printf("Hunt: %s\nSize: %ld bytes\nLast Modified: %s\n", hunt_id, st.st_size, ctime(&st.st_mtime));

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }

    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        printf("ID: %u, User: %s, Location: (%lf, %lf), Value: %u\nClue: %s\n\n",
               t.id, t.username, t.latitude, t.longitude, t.value, t.clue);
    }
    close(fd);

    log_operation(hunt_id, "LIST treasures");
}

void view_treasure(const char *hunt_id, uint32_t id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/treasures", hunt_id);

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }

    Treasure t;
    int found = 0;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (t.id == id) {
            printf("ID: %u, User: %s, Location: (%lf, %lf), Value: %u\nClue: %s\n",
                   t.id, t.username, t.latitude, t.longitude, t.value, t.clue);
            found = 1;
            break;
        }
    }
    close(fd);

    if (found) {
        log_operation(hunt_id, "VIEW treasure");
    } else {
        printf("Treasure not found.\n");
    }
}

void remove_treasure(const char *hunt_id, uint32_t id) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/treasures", hunt_id);

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }

    int temp_fd = open("tempfile", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (temp_fd == -1) {
        perror("open temp");
        close(fd);
        return;
    }

    Treasure t;
    int found = 0;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (t.id != id) {
            write(temp_fd, &t, sizeof(Treasure));
        } else {
            found = 1;
        }
    }
    close(fd);
    close(temp_fd);

    if (found) {
        unlink(file_path);
        rename("tempfile", file_path);
        log_operation(hunt_id, "REMOVE treasure");
    } else {
        printf("Treasure not found.\n");
        unlink("tempfile");
    }
}

void remove_hunt(const char *hunt_id) {
    char path[256];

    snprintf(path, sizeof(path), "%s/treasures", hunt_id);
    if (unlink(path) == -1) perror("unlink treasures");

    snprintf(path, sizeof(path), "%s/logged_hunt", hunt_id);
    if (unlink(path) == -1) perror("unlink logged_hunt");

    if (rmdir(hunt_id) == -1) {
        perror("rmdir hunt");
    } else {
        printf("Hunt '%s' removed successfully.\n", hunt_id);
        log_operation(".", "REMOVE_HUNT");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <command> <hunt_id> [<treasure_id>]\n", argv[0]);
        return 1;
    }

    const char *command = argv[1];
    const char *hunt_id = argv[2];

    if (strcmp(command, "--add") == 0) {
        if (argc == 3) {
            add_treasure(hunt_id);
        } else {
            printf("Invalid usage for --add.\n");
        }
    } else if (strcmp(command, "--list") == 0) {
        list_treasures(hunt_id);
    } else if (strcmp(command, "--view") == 0 && argc == 4) {
        view_treasure(hunt_id, (uint32_t)atoi(argv[3]));
    } else if (strcmp(command, "--remove_treasure") == 0 && argc == 4) {
        remove_treasure(hunt_id, (uint32_t)atoi(argv[3]));
    } else if (strcmp(command, "--remove_hunt") == 0) {
        remove_hunt(hunt_id);
    } else {
        printf("Invalid command.\n");
        return 1;
    }

    return 0;
}

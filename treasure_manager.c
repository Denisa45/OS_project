#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

#define MAX_PATH 256
#define MAX_USERNAME 50
#define MAX_CLUE_TEXT 500
#define LOG_FILENAME "logged_hunt"

typedef struct {
    int treasure_id;
    char username[MAX_USERNAME];
    double latitude;
    double longitude;
    char clue[MAX_CLUE_TEXT];
    int value;
} Treasure;

void add_treasure(const char* hunt_id);
void list_treasures(const char* hunt_id);
void view_treasure(const char* hunt_id, int treasure_id);
void remove_treasure(const char* hunt_id, int treasure_id);
void remove_hunt(const char* hunt_id);
void log_operation(const char* hunt_id, const char* operation);
void create_symlink(const char* hunt_id);
int get_next_treasure_id(const char* hunt_id);
int does_hunt_exist(const char* hunt_id);
int create_hunt_directory(const char* hunt_id);

int main() {
    char hunt_id[MAX_PATH];
    int choice;
    int treasure_id;

    printf("Enter hunt ID: ");
    scanf("%255s", hunt_id);

    while (1) {
        printf("\n--- Treasure Hunt Menu ---\n");
        printf("1. Add treasure\n");
        printf("2. List treasures\n");
        printf("3. View treasure\n");
        printf("4. Remove treasure\n");
        printf("5. Remove hunt\n");
        printf("6. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                add_treasure(hunt_id);
                break;
            case 2:
                list_treasures(hunt_id);
                break;
            case 3:
                printf("Enter treasure ID to view: ");
                scanf("%d", &treasure_id);
                view_treasure(hunt_id, treasure_id);
                break;
            case 4:
                printf("Enter treasure ID to remove: ");
                scanf("%d", &treasure_id);
                remove_treasure(hunt_id, treasure_id);
                break;
            case 5:
                remove_hunt(hunt_id);
                // After removing hunt, maybe ask for new hunt ID or exit
                printf("Enter new hunt ID or 'exit' to quit: ");
                scanf("%255s", hunt_id);
                if (strcmp(hunt_id, "exit") == 0) {
                    return 0;
                }
                break;
            case 6:
                printf("Exiting...\n");
                return 0;
            default:
                printf("Invalid choice, try again.\n");
        }
    }

    return 0;
}


int does_hunt_exist(const char* hunt_id) {
    char path[MAX_PATH];
    struct stat st;

    snprintf(path, MAX_PATH, "%s", hunt_id);

    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 1;
    }
    return 0;
}

int create_hunt_directory(const char* hunt_id) {
    char path[MAX_PATH];

    snprintf(path, MAX_PATH, "%s", hunt_id);

    if (mkdir(path, 0755) != 0) {
        perror("Failed to create hunt directory");
        return 0;
    }
    return 1;
}

int get_next_treasure_id(const char* hunt_id) {
    char path[MAX_PATH];
    int fd;
    Treasure treasure;
    int max_id = 0;

    snprintf(path, MAX_PATH, "%s/treasures.dat", hunt_id);

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        return 1;
    }

    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        if (treasure.treasure_id > max_id) {
            max_id = treasure.treasure_id;
        }
    }

    close(fd);
    return max_id + 1;
}

void add_treasure(const char* hunt_id) {
    char path[MAX_PATH];
    int fd;
    Treasure new_treasure;
    char log_msg[1024];

    if (!does_hunt_exist(hunt_id)) {
        if (!create_hunt_directory(hunt_id)) {
            return;
        }
    }

    new_treasure.treasure_id = get_next_treasure_id(hunt_id);

    printf("Enter username: ");
    scanf("%49s", new_treasure.username);

    printf("Enter latitude: ");
    scanf("%lf", &new_treasure.latitude);

    printf("Enter longitude: ");
    scanf("%lf", &new_treasure.longitude);

    printf("Enter clue text: ");
    getchar();
    fgets(new_treasure.clue, MAX_CLUE_TEXT, stdin);
    if (new_treasure.clue[strlen(new_treasure.clue) - 1] == '\n') {
        new_treasure.clue[strlen(new_treasure.clue) - 1] = '\0';
    }

    printf("Enter treasure value: ");
    scanf("%d", &new_treasure.value);

    snprintf(path, MAX_PATH, "%s/treasures.dat", hunt_id);

    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Failed to open treasures file");
        return;
    }

    if (write(fd, &new_treasure, sizeof(Treasure)) != sizeof(Treasure)) {
        perror("Failed to write treasure data");
        close(fd);
        return;
    }

    close(fd);

    snprintf(log_msg, sizeof(log_msg), "Added treasure ID %d by user %s",
        new_treasure.treasure_id, new_treasure.username);
    log_operation(hunt_id, log_msg);

    printf("Treasure added successfully with ID %d\n", new_treasure.treasure_id);
}

void view_treasure(const char* hunt_id, int treasure_id) {
    char path[MAX_PATH];
    int fd;
    Treasure treasure;
    int found = 0;
    char log_msg[1024];

    if (!does_hunt_exist(hunt_id)) {
        fprintf(stderr, "Hunt does not exist: %s\n", hunt_id);
        return;
    }

    snprintf(path, MAX_PATH, "%s/treasures.dat", hunt_id);

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open treasures file");
        return;
    }

    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        if (treasure.treasure_id == treasure_id) {
            printf("Treasure ID: %d\n", treasure.treasure_id);
            printf("Username: %s\n", treasure.username);
            printf("Location: %.6f, %.6f\n", treasure.latitude, treasure.longitude);
            printf("Clue: %s\n", treasure.clue);
            printf("Value: %d\n", treasure.value);
            found = 1;
            break;
        }
    }

    close(fd);

    if (!found) {
        fprintf(stderr, "Treasure not found with ID: %d\n", treasure_id);
    }

    snprintf(log_msg, sizeof(log_msg), "Viewed treasure ID %d (%s)",
        treasure_id, found ? "found" : "not found");
    log_operation(hunt_id, log_msg);
}

void log_operation(const char* hunt_id, const char* operation) {
    char log_path[MAX_PATH];
    int log_fd;
    time_t current_time;
    char time_str[50];
    char log_entry[1024];

    snprintf(log_path, MAX_PATH, "%s/%s", hunt_id, LOG_FILENAME);

    log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd == -1) {
        perror("Failed to open log file");
        return;
    }

    current_time = time(NULL);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&current_time));

    snprintf(log_entry, sizeof(log_entry), "[%s] %s\n", time_str, operation);
    write(log_fd, log_entry, strlen(log_entry));

    close(log_fd);

    create_symlink(hunt_id);
}

void create_symlink(const char* hunt_id) {
    char target_path[MAX_PATH];
    char link_path[MAX_PATH];

    snprintf(target_path, MAX_PATH, "%s/%s", hunt_id, LOG_FILENAME);
    snprintf(link_path, MAX_PATH, "logged_hunt-%s", hunt_id);

    unlink(link_path);//Deletes any existing symbolic link or file with the same name (logged_hunt-game1) to avoid failure when creating the new one.

    if (symlink(target_path, link_path) != 0) {
        perror("Failed to create symbolic link");
    }
}

void list_treasures(const char* hunt_id) {
    char path[MAX_PATH];
    int fd;
    Treasure treasure;
    struct stat file_stat;
    char time_str[50];
    char log_msg[1024];

    if (!does_hunt_exist(hunt_id)) {
        fprintf(stderr, "Hunt does not exist: %s\n", hunt_id);
        return;
    }

    snprintf(path, MAX_PATH, "%s/treasures.dat", hunt_id);

    if (stat(path, &file_stat) == -1) {
        if (errno == ENOENT) {
            printf("Hunt: %s\n", hunt_id);
            printf("No treasures found in this hunt\n");

            snprintf(log_msg, sizeof(log_msg), "Listed treasures (none found)");
            log_operation(hunt_id, log_msg);
            return;
        }
        perror("Failed to get file information");
        return;
    }

    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_mtime));

    printf("Hunt: %s\n", hunt_id);
    printf("File size: %ld bytes\n", (long)file_stat.st_size);
    printf("Last modification time: %s\n", time_str);
    printf("\nTreasures:\n");

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open treasures file");
        return;
    }

    int count = 0;
    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        printf("ID: %d, User: %s, Value: %d\n",
            treasure.treasure_id, treasure.username, treasure.value);
        count++;
    }

    close(fd);

    if (count == 0) {
        printf("No treasures found in this hunt\n");
    }

    snprintf(log_msg, sizeof(log_msg), "Listed treasures (%d found)", count);
    log_operation(hunt_id, log_msg);
}

void remove_treasure(const char* hunt_id, int treasure_id) {
    char path[MAX_PATH];
    char temp_path[MAX_PATH];
    int fd_in, fd_out;
    Treasure treasure;
    int found = 0;
    char log_msg[1024];

    if (!does_hunt_exist(hunt_id)) {
        fprintf(stderr, "Hunt does not exist: %s\n", hunt_id);
        return;
    }

    snprintf(path, MAX_PATH, "%s/treasures.dat", hunt_id);
    snprintf(temp_path, MAX_PATH, "%s/treasures.tmp", hunt_id);

    fd_in = open(path, O_RDONLY);
    if (fd_in == -1) {
        perror("Failed to open treasures file");
        return;
    }

    fd_out = open(temp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1) {
        perror("Failed to create temporary file");
        close(fd_in);
        return;
    }

    while (read(fd_in, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        if (treasure.treasure_id == treasure_id) {
            found = 1;
            continue;
        }
        if (write(fd_out, &treasure, sizeof(Treasure)) != sizeof(Treasure)) {
            perror("Failed to write to temporary file");
            close(fd_in);
            close(fd_out);
            unlink(temp_path);
            return;
        }
    }

    close(fd_in);
    close(fd_out);

    if (!found) {
        fprintf(stderr, "Treasure not found with ID: %d\n", treasure_id);
        unlink(temp_path);
        return;
    }

    if (rename(temp_path, path) != 0) {
        perror("Failed to replace treasures file");
        return;
    }

    snprintf(log_msg, sizeof(log_msg), "Removed treasure ID %d", treasure_id);
    log_operation(hunt_id, log_msg);

    printf("Treasure removed successfully\n");
}

void remove_hunt(const char* hunt_id) {
    char command[MAX_PATH + 10];
    char log_msg[1024];
    char link_path[MAX_PATH];

    if (!does_hunt_exist(hunt_id)) {
        fprintf(stderr, "Hunt does not exist: %s\n", hunt_id);
        return;
    }

    snprintf(log_msg, sizeof(log_msg), "Removed hunt %s", hunt_id);
    log_operation(hunt_id, log_msg);

    snprintf(link_path, MAX_PATH, "logged_hunt-%s", hunt_id);
    unlink(link_path);

    snprintf(command, sizeof(command), "rm -rf %s", hunt_id);
    system(command);

    printf("Hunt removed successfully\n");
}
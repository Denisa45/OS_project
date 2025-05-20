#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>

#define MAX_PATH 256
#define MAX_USERNAME 50
#define MAX_CLUE_TEXT 500
#define MAX_COMMAND 1024
#define DELAY_MS 500000

typedef struct {
    int treasure_id;
    char username[MAX_USERNAME];
    double latitude;
    double longitude;
    char clue[MAX_CLUE_TEXT];
    int value;
} Treasure;

pid_t monitor_pid = -1;
int monitor_running = 0;
int received_signal = 0;
int exit_requested = 0;


void parent_signal_handler(int signum);
void process_command(const char* command);
void start_monitor();
void stop_monitor();
void monitor_process();
void monitor_signal_handler(int signum);
void list_all_hunts();
void list_hunt_treasures(const char* hunt_id);
void view_hunt_treasure(const char* hunt_id, int treasure_id);
int count_treasures(const char* hunt_id);
int does_hunt_exist(const char* hunt_id);


void run_menu();

int main() {
    struct sigaction sa;

    sa.sa_handler = parent_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    run_menu();

    return 0;
}

void run_menu() {
    printf("Treasure Hub - Interactive Interface\n");
    printf("------------------------------------\n");
    printf("Available commands:\n");
    printf("  start_monitor\n");
    printf("  list_hunts\n");
    printf("  list_treasures <hunt_id>\n");
    printf("  view_treasure <hunt_id> <treasure_id>\n");
    printf("  stop_monitor\n");
    printf("  exit\n");

    char command[MAX_COMMAND];

    while (1) {
        printf("\n> ");
        if (fgets(command, MAX_COMMAND, stdin) == NULL) {
            break;
        }

        command[strcspn(command, "\n")] = '\0';
        process_command(command);
    }
}

void parent_signal_handler(int signum) {
    //If SIGUSR1 → the monitor has completed a task 
    if (signum == SIGUSR1) {
        printf("Task done!\n");
    //If SIGUSR2 → the monitor has terminated
    } else if (signum == SIGUSR2) {
        monitor_running = 0;
    }
}

void process_command(const char* command) {
    if (strcmp(command, "start_monitor") == 0) {
        start_monitor();
    } else if (strcmp(command, "list_hunts") == 0) {
        if (!monitor_running) {
            printf("Error: Monitor is not running. Start monitor first.\n");
            return;
        }
        kill(monitor_pid, SIGUSR1);
        pause();
    } else if (strncmp(command, "list_treasures", 14) == 0) {
        if (!monitor_running) {
            printf("Error: Monitor is not running. Start monitor first.\n");
            return;
        }

        char hunt_id[MAX_PATH];
        if (sscanf(command, "list_treasures %255s", hunt_id) != 1) {
            printf("Usage: list_treasures <hunt_id>\n");
            return;
        }

        FILE* tmp = fopen("temp_command.txt", "w");
        if (tmp) {
            fprintf(tmp, "%s", hunt_id);
            fclose(tmp);
            kill(monitor_pid, SIGUSR2);
            pause();
        } else {
            perror("Failed to create temporary file");
        }
    } else if (strncmp(command, "view_treasure", 13) == 0) {
        if (!monitor_running) {
            printf("Error: Monitor is not running. Start monitor first.\n");
            return;
        }

        char hunt_id[MAX_PATH];
        int treasure_id;
        if (sscanf(command, "view_treasure %255s %d", hunt_id, &treasure_id) != 2) {
            printf("Usage: view_treasure <hunt_id> <treasure_id>\n");
            return;
        }

        FILE* tmp = fopen("temp_command.txt", "w");
        if (tmp) {
            fprintf(tmp, "%s %d", hunt_id, treasure_id);
            fclose(tmp);
            kill(monitor_pid, SIGINT);
            pause();
        } else {
            perror("Failed to create temporary file");
        }
    } else if (strcmp(command, "stop_monitor") == 0) {
        stop_monitor();
    } else if (strcmp(command, "exit") == 0) {
        if (monitor_running) {
            printf("Error: Monitor is still running. Stop monitor first.\n");
        } else {
            exit(0);
        }
    } else {
        printf("Unknown command: %s\n", command);
        printf("Available commands: start_monitor, list_hunts, list_treasures <hunt_id>, view_treasure <hunt_id> <treasure_id>, stop_monitor, exit\n");
    }
}

void start_monitor() {
    if (monitor_running) {
        printf("Monitor is already running.\n");
        return;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(1);
        // child makes the work in the monitor_process function
    } else if (pid == 0) {
        monitor_process();
        exit(0);
    } else {
        // parent process Waits for user input, sends signals to monitor
        monitor_pid = pid;
        monitor_running = 1;
        printf("Monitor started with PID: %d\n", monitor_pid);

        pause();
    }
}

void stop_monitor() {
    if (!monitor_running) {
        printf("Monitor is not running.\n");
        return;
    }

    kill(monitor_pid, SIGTERM);

    printf("Waiting for monitor to terminate...\n");

    int status;
    waitpid(monitor_pid, &status, 0);

    if (WIFEXITED(status)) {
        printf("Monitor terminated with exit status: %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("Monitor terminated by signal: %d\n", WTERMSIG(status));
    }

    monitor_running = 0;
    monitor_pid = -1;
}
// perform a task based on the signal received
void monitor_process() {
    struct sigaction sa;

    sa.sa_handler = monitor_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    //When signal SIGUSR1 occurs, run monitor_signal_handler with the specified mask and flags.
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    //Send SIGUSR1 to the parent process to indicate that the monitor is ready.
    kill(getppid(), SIGUSR1);

    char hunt_id[MAX_PATH];
    int treasure_id;

    while (!exit_requested) {
        //Wait for a signal to be received
        pause();

        if (received_signal == SIGUSR1) {
            list_all_hunts();
            kill(getppid(), SIGUSR1);
        } else if (received_signal == SIGUSR2) {
            
            FILE* tmp = fopen("temp_command.txt", "r");
            if (tmp) {
                if (fscanf(tmp, "%255s", hunt_id) == 1) {
                    fclose(tmp);
                    //
                    list_hunt_treasures(hunt_id);
                } else {
                    fclose(tmp);
                    printf("Error reading hunt ID from temporary file\n");
                }
            }
            //
            kill(getppid(), SIGUSR1);//The child tells the parent, "I finished processing your request (signal)."
        } else if (received_signal == SIGINT) {
            FILE* tmp = fopen("temp_command.txt", "r");
            if (tmp) {
                if (fscanf(tmp, "%255s %d", hunt_id, &treasure_id) == 2) {
                    fclose(tmp);
                    view_hunt_treasure(hunt_id, treasure_id);
                } else {
                    fclose(tmp);
                    printf("Error reading parameters from temporary file\n");
                }
            }
            kill(getppid(), SIGUSR1);
        } else if (received_signal == SIGTERM) {
            exit_requested = 1;
        }

        received_signal = 0;
    }

    usleep(DELAY_MS);

    kill(getppid(), SIGUSR2);
    exit(0);
}
//Record which signal it received
void monitor_signal_handler(int signum) {
    received_signal = signum;
}

void list_all_hunts() {
    DIR* dir;
    struct dirent* entry;
    struct stat st;
    char path[MAX_PATH * 2];

    dir = opendir(".");// Open the current directory
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    printf("Available Hunts:\n");
    printf("---------------\n");
    int hunt_count = 0;

   while ((entry = readdir(dir)) != NULL) { // Read each entry in the directory
    // Skip the current and parent directory entries
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
        continue;
    }

    // Check if the entry is a directory
    if (stat(entry->d_name, &st) == 0 && S_ISDIR(st.st_mode)) {
        
        // Check if the combined path length will fit in MAX_PATH buffer
        if (strlen(entry->d_name) + 14 < MAX_PATH) {
            // Construct a path like "<dirname>/treasures.dat"
            snprintf(path, MAX_PATH * 2, "%s/treasures.dat", entry->d_name);

            // Check if the file "treasures.dat" exists in that directory
            if (access(path, F_OK) != -1) {
                // Count treasures in this hunt directory
                int count = count_treasures(entry->d_name);
                printf("Hunt: %s - Total treasures: %d\n", entry->d_name, count);
                hunt_count++;
            }
        } else {
            printf("Hunt name too long: %s\n", entry->d_name);
        }
    }
}


    if (hunt_count == 0) {
        printf("No hunts found.\n");
    }

    closedir(dir);
}

void list_hunt_treasures(const char* hunt_id) {
    char path[MAX_PATH];
    int fd;
    Treasure treasure;
    struct stat file_stat;
    char time_str[50];

    if (!does_hunt_exist(hunt_id)) {
        printf("Hunt does not exist: %s\n", hunt_id);
        return;
    }

    if (strlen(hunt_id) + 14 >= MAX_PATH) {
        printf("Hunt ID too long: %s\n", hunt_id);
        return;
    }

    snprintf(path, MAX_PATH, "%s/treasures.dat", hunt_id);

    if (stat(path, &file_stat) == -1) {
        if (errno == ENOENT) {
            printf("Hunt: %s\n", hunt_id);
            printf("No treasures found in this hunt\n");
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
}

void view_hunt_treasure(const char* hunt_id, int treasure_id) {
    char path[MAX_PATH];
    int fd;
    Treasure treasure;
    int found = 0;

    if (!does_hunt_exist(hunt_id)) {
        printf("Hunt does not exist: %s\n", hunt_id);
        return;
    }

    if (strlen(hunt_id) + 14 >= MAX_PATH) {
        printf("Hunt ID too long: %s\n", hunt_id);
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
        printf("Treasure not found with ID: %d\n", treasure_id);
    }
}

int does_hunt_exist(const char* hunt_id) {
    struct stat st;
    if (stat(hunt_id, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 1;
    }
    return 0;
}

int count_treasures(const char* hunt_id) {
    char path[MAX_PATH];
    int fd;
    Treasure treasure;
    int count = 0;

    snprintf(path, MAX_PATH, "%s/treasures.dat", hunt_id);

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        return 0;
    }

    while (read(fd, &treasure, sizeof(Treasure)) == sizeof(Treasure)) {
        count++;
    }

    close(fd);
    return count;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

#define COMMAND_FILE "hub_command.txt"

pid_t monitor_pid = -1;
int monitor_running = 0;
int monitor_exiting = 0;

void handle_sigchld(int sig) {
    int status;
    pid_t pid = waitpid(monitor_pid, &status, WNOHANG);
    if (pid == monitor_pid) {
        monitor_running = 0;
        monitor_exiting = 0;
        printf("[INFO] Monitor process terminated with status %d\n", status);
    }
}

void write_command(const char *cmd) {
    int fd = open(COMMAND_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open command file");
        return;
    }
    write(fd, cmd, strlen(cmd));
    close(fd);
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGCHLD, &sa, NULL);

    char input[256];
    while (1) {
        printf("hub> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0; // trim newline

        if (strcmp(input, "start_monitor") == 0) {
            if (monitor_running) {
                printf("Monitor is already running.\n");
                continue;
            }
            monitor_pid = fork();
            if (monitor_pid == 0) {
                execl("./monitor", "monitor", NULL);
                perror("execl");
                exit(1);
            } else {
                monitor_running = 1;
                printf("Monitor started with PID %d\n", monitor_pid);
            }
        } else if (strcmp(input, "list_hunts") == 0 ||
                   strcmp(input, "list_treasures") == 0 ||
                   strncmp(input, "view_treasure", 13) == 0) {
            if (!monitor_running) {
                printf("Monitor is not running.\n");
                continue;
            }
            if (monitor_exiting) {
                printf("Monitor is exiting. Please wait.\n");
                continue;
            }
            write_command(input);
            kill(monitor_pid, SIGUSR1);
        } else if (strcmp(input, "stop_monitor") == 0) {
            if (!monitor_running) {
                printf("Monitor is not running.\n");
                continue;
            }
            monitor_exiting = 1;
            write_command("stop_monitor");
            kill(monitor_pid, SIGUSR1);
        } else if (strcmp(input, "exit") == 0) {
            if (monitor_running) {
                printf("Cannot exit. Monitor is still running.\n");
                continue;
            }
            break;
        } else {
            printf("Unknown command.\n");
        }
    }
    return 0;
}

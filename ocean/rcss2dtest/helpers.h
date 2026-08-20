#define _GNU_SOURCE

#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

pid_t process_launch_async(const char *command, char *const args[], char *const env[], const char *redirect_path) {
    if (!command || !args) {
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("process_helper: fork failed");
        return -1;
    }

    if (pid == 0) {
        // Redirect stdout + stderr to the given path (supports "/dev/null" or any file)
        if (redirect_path != NULL) {
            int fd = open(redirect_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd >= 0) {
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }

        // Inside Child Process
        if (env != NULL) {
            execvpe(command, args, env);
        } else {
            execvp(command, args);
        }
        
        // If execvp/execvpe returns, an error occurred
        perror("process_helper: execvp/execvpe failed");
        _exit(EXIT_FAILURE); 
    }

    // Inside Parent Process
    return pid;
}

int process_launch_sync(const char *command, char *const args[], char *const env[], const char *redirect_path) {
    pid_t pid = process_launch_async(command, args, env, redirect_path);
    if (pid < 0) {
        return -1;
    }

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("process_helper: waitpid failed");
        return -1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return -1;
}

bool process_reap(pid_t pid, int *exit_status) {
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        return false;
    }

    if (exit_status && WIFEXITED(status)) {
        *exit_status = WEXITSTATUS(status);
    }
    return true;
}

void process_reap_all_any(void) {
    // waitpid with -1 and WNOHANG reaps all dead children without blocking
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int process_send_sigint(pid_t pid) {
    // kill() returns 0 on success, -1 on failure
    if (kill(pid, SIGINT) == 0) {
        return 0; 
    } else {
        return -1;
    }
}

// Check whether a UDP port appears free on INADDR_ANY.
// We use this to avoid blindly launching rcssserver on a port that is in use.
static bool port_is_free(unsigned int port) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return false;

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);

    int res = bind(s, (struct sockaddr *)&a, sizeof(a));
    close(s);
    return res == 0;
}

// Returns a random free UDP port in a reasonable range
static unsigned int get_random_free_port(unsigned int * rng) {
    const unsigned int min_port = 6000;
    const unsigned int max_port = 9999;
    const int max_tries = 300;

    for (int i = 0; i < max_tries; i++) {
        unsigned int r = rand_r(rng);
        unsigned int port = min_port + (r % (max_port - min_port + 1));

        if (port_is_free(port)) {
            return port;
        }
    }
    return 0;  // could not find a free port
}

int get_ports_for_rcss(unsigned int * player_port, unsigned int * coach_port, unsigned int * offline_coach_port, unsigned int * rng) {
    // Acquire three distinct free ports by calling the rng-based helper.
    unsigned int tmp_player_port = 0;
    unsigned int tmp_coach_port = 0;
    unsigned int tmp_offline_coach_port = 0;

    int port_tries = 0;
    while (port_tries < 50) {
        tmp_player_port = get_random_free_port(rng);
        tmp_coach_port = get_random_free_port(rng);
        tmp_offline_coach_port = get_random_free_port(rng);

        if (tmp_player_port && tmp_coach_port && tmp_offline_coach_port &&
            tmp_player_port != tmp_coach_port &&
            tmp_player_port != tmp_offline_coach_port &&
            tmp_coach_port != tmp_offline_coach_port) {
            break;
        }
        port_tries++;
    }

    if (!tmp_player_port || !tmp_coach_port || !tmp_offline_coach_port) {
        printf("failed to find three distinct free ports\n");
        return -1;
    }

    *player_port = tmp_player_port;
    *coach_port = tmp_coach_port;
    *offline_coach_port = tmp_offline_coach_port;

    return 0;
}

void msleep(long milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

#define RCSSSERVER_BINARY_PATH "/home/babaeti2/rcssserver/build/rcssserver"
#define RCSSMONITOR_BINARY_PATH "/home/babaeti2/rcssmonitor/build/rcssmonitor"
#define SAMPLE_PLAYER_BINARY_PATH "/home/babaeti2/helios/helios-base/build/bin/sample_player"
#define SAMPLE_COACH_BINARY_PATH "/home/babaeti2/helios/helios-base/build/bin/sample_coach"

pid_t launch_server(unsigned int player_port, unsigned int coach_port, unsigned int offline_coach_port, unsigned int seed, const char *log_path, const char *text_log_name) {
    char player_port_arg[256];
    char coach_port_arg[256];
    char offline_coach_port_arg[256];
    char server_seed_arg[256];
    char player_seed_arg[256];
    char text_log_fixed_name_arg[256];
    sprintf(player_port_arg, "server::port=%u", player_port);
    sprintf(coach_port_arg, "server::olcoach_port=%u", coach_port);
    sprintf(offline_coach_port_arg, "server::coach_port=%u", offline_coach_port);
    sprintf(server_seed_arg, "server::random_seed=%u", seed);
    sprintf(player_seed_arg, "player::random_seed=%u", seed);
    sprintf(text_log_fixed_name_arg, "server::text_log_fixed_name=%s", text_log_name);
    
    const char *const args[] = {
        RCSSSERVER_BINARY_PATH,
        player_port_arg,
        coach_port_arg,
        offline_coach_port_arg,
        "server::synch_mode=on", 
        "server::profile=on",
        "server::auto_mode=on",
        "server::game_logging=off",
        "server::text_logging=off",
        // "server::text_log_fixed=on",
        // text_log_fixed_name_arg,
        "server::prerun_wait=0",
        "server::kick_off_wait=10",
        "server::connect_wait=30",
        "server::half_time=-1",
        "server::coach_w_referee=on",
        // "server::synch_micro_sleep=50",
        server_seed_arg,
        player_seed_arg,
        NULL
    };
    pid_t pid = process_launch_async(RCSSSERVER_BINARY_PATH, (char *const *)args, NULL, log_path);
    // printf("started rcssserver with PID=%d\n", pid);
    // fflush(stdout);
    return pid;
}

pid_t launch_monitor(unsigned int server_port, const char *log_path) {
    char server_port_arg[256];
    sprintf(server_port_arg, "%u", server_port);
    const char *const args[] = {
        RCSSMONITOR_BINARY_PATH,
        "--server-port", server_port_arg,
        "--auto-quit-mode", "true",
        NULL
    };
    pid_t pid = process_launch_async(RCSSMONITOR_BINARY_PATH, (char *const *)args, NULL, log_path);
    // printf("started rcssmonitor with PID=%d\n", pid);
    // fflush(stdout);
    return pid;
}

pid_t launch_player(const char* host, unsigned int port, const char* teamname, bool is_goalie, const char *log_path) {
    char player_port_arg[256];
    sprintf(player_port_arg, "%u", port);

    const char *args[16];
    int n = 0;
    args[n++] = SAMPLE_PLAYER_BINARY_PATH;
    args[n++] = "--player-config";
    args[n++] = "/home/babaeti2/helios/helios-base/build/bin/player.conf";
    args[n++] = "--config_dir";
    args[n++] = "/home/babaeti2/helios/helios-base/build/bin/formations-dt";
    args[n++] = "-h";
    args[n++] = host;
    args[n++] = "-p";
    args[n++] = player_port_arg;
    args[n++] = "-t";
    args[n++] = teamname;
    if (is_goalie) {
        args[n++] = "-g";
    }
    args[n++] = NULL;

    const char *const env_vars[] = {"LD_LIBRARY_PATH=/home/babaeti2/helios/librcsc/out/lib", NULL};
    pid_t pid = process_launch_async(SAMPLE_PLAYER_BINARY_PATH, (char *const *)args, (char *const *)env_vars, log_path);
    // printf("started %s with PID=%d\n", is_goalie ? "goalie" : "player", pid);
    // fflush(stdout);
    return pid;
}

pid_t launch_coach(const char* host, unsigned int port, const char* teamname, const char *log_path) {
    char coach_port_arg[256];
    sprintf(coach_port_arg, "%u", port);
    const char *const args[] = {
        SAMPLE_COACH_BINARY_PATH,
        "--coach-config", "/home/babaeti2/helios/helios-base/build/bin/coach.conf", 
        "-h", host, 
        "-p", coach_port_arg, 
        "-t", teamname, 
        "--use_team_graphic", "off"
    };
    const char *const env_vars[] = {"LD_LIBRARY_PATH=/home/babaeti2/helios/librcsc/out/lib", NULL};
    pid_t pid = process_launch_async(SAMPLE_COACH_BINARY_PATH, (char *const *)args, (char *const *)env_vars, log_path);
    // printf("started coach with PID=%d\n", pid);
    // fflush(stdout);
    return pid;
}

int launch_team(const char* host, unsigned int player_port, unsigned int coach_port, const char* teamname, pid_t* pid_list, const char *log_path) {
    pid_list[0] = launch_player(host, player_port, teamname, true, log_path);
    msleep(100);
    for (int i = 1 ; i < 11 ; i++) {
        pid_list[i] = launch_player(host, player_port, teamname, false, log_path);
        msleep(100);
    }
    pid_list[11] = launch_coach(host, coach_port, teamname, log_path);
    msleep(100);
    return 0;
}

void get_current_timestamp_str(char * buffer, unsigned int max_size) {
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    strftime(buffer, max_size, "%Y%m%d%H%M%S", local_time);
}
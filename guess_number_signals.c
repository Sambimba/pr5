#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <errno.h>

#define MAX_ATTEMPTS 100
#define GAME_CYCLES 10

typedef struct {
    int number;
    int guess;
    int attempts;
    bool is_correct;
    bool is_thinker;
} GameData;

volatile sig_atomic_t game_over = 0;
volatile GameData game;

void flush_output() {
    fflush(stdout);
}

void sigint_handler(int sig) {
    (void)sig; // Явно указываем, что параметр не используется
    game_over = 1;
}

void thinker_handler(int sig, siginfo_t *info, void *ucontext) {
    (void)ucontext; // Неиспользуемый параметр
    if (sig == SIGUSR1) {
        game.guess = info->si_value.sival_int;
        game.attempts++;
        
        if (game.guess == game.number) {
            game.is_correct = true;
            printf("Thinker: Correct! %d in %d attempts\n", game.guess, game.attempts);
        } else {
            game.is_correct = false;
            printf("Thinker: Wrong guess %d (attempt %d)\n", game.guess, game.attempts);
        }
        flush_output();
        
        // Send response
        union sigval value;
        value.sival_int = game.is_correct;
        sigqueue(info->si_pid, SIGUSR2, value);
    }
}

void guesser_handler(int sig, siginfo_t *info, void *ucontext) {
    (void)ucontext; // Неиспользуемый параметр
    if (sig == SIGUSR2) {
        game.is_correct = info->si_value.sival_int;
        
        if (game.is_correct) {
            printf("Guesser: Correct! Number was %d (attempts: %d)\n", 
                  game.guess, game.attempts);
            game.is_thinker = !game.is_thinker; // Switch roles
        } else {
            printf("Guesser: Wrong! Attempt %d: %d\n", game.attempts, game.guess);
        }
        flush_output();
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <N>\n", argv[0]);
        return 1;
    }

    int N = atoi(argv[1]);
    if (N <= 0) {
        fprintf(stderr, "N must be positive\n");
        return 1;
    }

    signal(SIGINT, sigint_handler);

    // Initialize game data
    game.number = 0;
    game.guess = 0;
    game.attempts = 0;
    game.is_correct = false;
    game.is_thinker = true; // Parent starts as thinker

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    srand(time(NULL) ^ (getpid() << 16));

    if (pid == 0) {
        // Child process
        struct sigaction sa;
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = guesser_handler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR2, &sa, NULL);

        printf("Child process started (pid: %d)\n", getpid());
        flush_output();

        game.is_thinker = false; // Child starts as guesser
    } else {
        // Parent process
        struct sigaction sa;
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = thinker_handler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR1, &sa, NULL);

        printf("Parent process started (pid: %d)\n", getpid());
        flush_output();
    }

    int cycles = 0;
    while (cycles < GAME_CYCLES && !game_over) {
        if (game.is_thinker) {
            // Thinker mode
            game.number = rand() % N + 1;
            game.attempts = 0;
            game.is_correct = false;
            printf("Thinker (pid %d): New number is %d\n", getpid(), game.number);
            flush_output();

            // Wait for correct guess
            while (!game_over && !game.is_correct) {
                pause();
            }
            
            if (game.is_correct) {
                cycles++;
                game.is_thinker = false; // Switch to guesser
                printf("--- Switching roles (cycle %d) ---\n", cycles);
            }
        } else {
            // Guesser mode
            game.guess = rand() % N + 1;
            game.attempts++;
            printf("Guesser (pid %d): Attempt %d - guessing %d\n", 
                  getpid(), game.attempts, game.guess);
            flush_output();

            // Send guess to thinker
            union sigval value;
            value.sival_int = game.guess;
            if (sigqueue(pid == 0 ? getppid() : pid, SIGUSR1, value) == -1) {
                perror("sigqueue");
                break;
            }

            // Wait for response
            pause();
            
            if (game.is_correct) {
                game.attempts = 0;
                game.is_thinker = true; // Switch to thinker
            }
        }
    }

    if (pid != 0) {
        kill(pid, SIGTERM);
        int status;
        waitpid(pid, &status, 0);
        printf("Game finished after %d rounds\n", cycles);
    } else {
        printf("Child process exiting\n");
    }
    flush_output();

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>

#define MAX_ATTEMPTS 100
#define GAME_CYCLES 2  // Для теста, можно увеличить

volatile sig_atomic_t number_to_guess = 0;
volatile sig_atomic_t current_guess = 0;
volatile sig_atomic_t attempts = 0;
volatile sig_atomic_t game_active = 1;

pid_t opponent_pid = 0;
int is_guesser = 0;

void handle_guess(int sig, siginfo_t *info, void *context) {
    current_guess = info->si_value.sival_int;
    attempts++;
}

void handle_result(int sig) {
    if (sig == SIGUSR1) {
        game_active = 0;  // Число угадано
    }
}

void send_guess(int guess) {
    union sigval value;
    value.sival_int = guess;
    sigqueue(opponent_pid, SIGRTMIN, value);
}

void setup_signals() {
    struct sigaction sa;
    
    // Обработчик для получения догадки
    sa.sa_sigaction = handle_guess;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGRTMIN, &sa, NULL);
    
    // Обработчики для результатов
    sa.sa_handler = handle_result;
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
}

void guesser(int max) {
    int low = 1, high = max, guess;
    while (game_active && attempts < MAX_ATTEMPTS) {
        guess = low + (high - low)/2;
        printf("PID %d guesses: %d\n", getpid(), guess);
        send_guess(guess);
        
        pause();  // Ждем ответ
        
        if (!game_active) {
            printf("PID %d won in %d attempts!\n", getpid(), attempts);
            break;
        } else {
            if (current_guess < number_to_guess) {
                low = guess + 1;
            } else {
                high = guess - 1;
            }
        }
    }
}

void thinker(int max) {
    number_to_guess = rand() % max + 1;
    printf("PID %d chose: %d\n", getpid(), number_to_guess);
    
    while (game_active && attempts < MAX_ATTEMPTS) {
        pause();  // Ждем догадку
        
        if (current_guess == number_to_guess) {
            printf("PID %d: correct!\n", getpid());
            kill(opponent_pid, SIGUSR1);
            game_active = 0;
        } else {
            printf("PID %d: wrong\n", getpid());
            kill(opponent_pid, SIGUSR2);
        }
    }
}

void play_round(int max, int role) {
    game_active = 1;
    attempts = 0;
    is_guesser = role;
    
    if (is_guesser) {
        guesser(max);
    } else {
        thinker(max);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <max_number>\n", argv[0]);
        return 1;
    }
    
    int max = atoi(argv[1]);
    if (max <= 0) {
        fprintf(stderr, "Number must be positive\n");
        return 1;
    }
    
    srand(time(NULL));
    setup_signals();
    
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }
    
    if (pid == 0) {  // Child
        opponent_pid = getppid();
        for (int i = 0; i < GAME_CYCLES; i++) {
            play_round(max, i % 2);
            sleep(1);  // Пауза между раундами
        }
        exit(0);
    } else {  // Parent
        opponent_pid = pid;
        for (int i = 0; i < GAME_CYCLES; i++) {
            play_round(max, (i + 1) % 2);
            sleep(1);  // Пауза между раундами
        }
        kill(pid, SIGTERM);
        wait(NULL);
    }
    
    return 0;
}

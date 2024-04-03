#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>

#define MAX_ROUNDS 20
#define MAX_SCORE 501

#define BASE 0
#define OPEN_PLAYERONE 1
#define OPEN_PLAYERTWO 2
#define CLOSED -1

sem_t *startSem, *player1Sem, *player2Sem;
int pipefd[2];
int player1Score = 0, player2Score = 0;
int player1Hits[7] = {0}, player2Hits[7] = {0};
int areaStates[7] = {BASE};

void delay() {
    int delay = rand() % 2 + 1;
    sleep(delay);
}

struct Throw {
    int area;
    int multiplier;
    int score;
};

struct Throw throwDart() {
    int area = rand() % 9 + 13;
    int multiplier = rand() % 3 + 1;
    int score = area;
    area -= 15;

    if (score == 21) {
        if (multiplier == 3) score = 50;
        else score = 25;
    } else score = area * multiplier;

    struct Throw throw = {area, multiplier, score};
    return throw;
}

void player(int playerID) {
    char name[20];
    sprintf(name, "Player %d", playerID);
    printf("%s has joined the game.\n", name);
    srand(time(NULL) * playerID);

    while (1) {
        sem_wait(startSem);
        delay();
        struct Throw throw = throwDart();
        printf("%s has thrown %d * %d.\n", name, throw.area, throw.multiplier);

        if (playerID == 1) {
            sem_wait(player1Sem);
            write(pipefd[1], &throw, sizeof(struct Throw));
            sem_post(player1Sem);
        } else {
            sem_wait(player2Sem);
            write(pipefd[1], &throw, sizeof(struct Throw));
            sem_post(player2Sem);
        }
    }
}

int main() {
    sem_unlink("/startSem");
    sem_unlink("/player1Sem");
    sem_unlink("/player2Sem");
    startSem = sem_open("/startSem", O_CREAT, 0644, 0);
    player1Sem = sem_open("/player1Sem", O_CREAT, 0644, 0);
    player2Sem = sem_open("/player2Sem", O_CREAT, 0644, 0);

    pipe(pipefd);

    pid_t player1PID = fork();
    if (player1PID == 0) {
        player(1);
        exit(0);
    }

    pid_t player2PID = fork();
    if (player2PID == 0) {
        player(2);
        exit(0);
    }

    int round = 1;
    while (round <= MAX_ROUNDS && player1Score < MAX_SCORE && player2Score < MAX_SCORE) {
        printf("Round %d\n", round);
        sem_post(startSem);
        sem_post(startSem);

        struct Throw throw1, throw2;

        sem_post(player1Sem);
        read(pipefd[0], &throw1, sizeof(struct Throw));
        sem_wait(player1Sem);

        if (throw1.area > 0 && areaStates[throw1.area] != CLOSED) {
            player1Hits[throw1.area] += throw1.multiplier;
            if (areaStates[throw1.area] == OPEN_PLAYERONE) player1Score += throw1.score;
            else if (player1Hits[throw1.area] >= 3) {
                if (areaStates[throw1.area] == BASE) {
                    areaStates[throw1.area] = OPEN_PLAYERONE;
                    printf("Player 1 opened area %d\n", throw1.area + 15);
                }
                if (areaStates[throw1.area] == OPEN_PLAYERTWO) {
                    areaStates[throw1.area] = CLOSED;
                    printf("Player 1 closed area %d\n", throw1.area + 15);
                }
            }
        }

        sem_post(player2Sem);
        read(pipefd[0], &throw2, sizeof(struct Throw));
        sem_wait(player2Sem);

        if (throw2.area > 0 && areaStates[throw2.area] != CLOSED) {
            player2Hits[throw2.area] += throw2.multiplier;
            if (areaStates[throw2.area] == OPEN_PLAYERTWO) player1Score += throw2.score;
            else if (player2Hits[throw2.area] >= 3) {
                if (areaStates[throw2.area] == BASE) {
                    areaStates[throw2.area] = OPEN_PLAYERTWO;
                    printf("Player 2 opened area %d\n", throw1.area + 15);
                }
                if (areaStates[throw2.area] == OPEN_PLAYERONE) {
                    areaStates[throw2.area] = CLOSED;
                    printf("Player 2 closed area %d\n", throw1.area + 15);
                }
            }
        }

        printf("Player 1 score: %d\n", player1Score);
        printf("Player 2 score: %d\n", player2Score);

        bool allClosed = true;
        for (int i = 0; i < 7; ++i) {
            if (areaStates[i] != CLOSED) {
                allClosed = false;
                break;
            }
        }

        if (allClosed) {
            printf("All areas are closed.\n");
            break;
        }

        round++;
    }

    kill(player1PID, SIGKILL);
    kill(player2PID, SIGKILL);

    if (player1Score >= player2Score) {
        printf("Player 1 wins!\n");
    } else if (player2Score >= player1Score) {
        printf("Player 2 wins!\n");
    } else {
        printf("It's a draw!\n");
    }

    sem_unlink("/startSem");
    sem_unlink("/player1Sem");
    sem_unlink("/player2Sem");

    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdbool.h>

#define MAX_ROUNDS 20
#define MAX_SCORE 501

#define BASE 0
#define OPEN_PLAYERONE 1
#define OPEN_PLAYERTWO 2
#define CLOSED (-1)

sem_t *startSem[2], *sendSem[2], *sentSem[2];
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

    if (score == 21) {
        if (multiplier == 3) score = 50;
        else score = 25;
    } else score = area * multiplier;

    area -= 15;

    struct Throw throw = {area, multiplier, score};
    return throw;
}

void signalHandler(int signal) {
    printf("Signal %d received.\n", signal);
}

void player(int playerID) {
    char name[20];
    sprintf(name, "Player %d", playerID + 1);
    printf("%s has joined the game.\n", name);
    srand(time(NULL) * (playerID + 1));

    while (true) {
        sem_wait(startSem[playerID]);
        delay();
        struct Throw throw = throwDart();
        printf("%s has thrown %d * %d.\n", name, throw.area + 15, throw.multiplier);

        sem_wait(sendSem[playerID]);
        write(pipefd[1], &throw, sizeof(struct Throw));
        sem_post(sentSem[playerID]);
    }
}

int main() {
    startSem[0] = sem_open("/startSem0", O_CREAT, S_IRUSR | S_IWUSR, 0);
    startSem[1] = sem_open("/startSem1", O_CREAT, S_IRUSR | S_IWUSR, 0);
    sendSem[0] = sem_open("/sendSem0", O_CREAT, S_IRUSR | S_IWUSR, 0);
    sendSem[1] = sem_open("/sendSem1", O_CREAT, S_IRUSR | S_IWUSR, 0);
    sentSem[0] = sem_open("/sentSem0", O_CREAT, S_IRUSR | S_IWUSR, 0);
    sentSem[1] = sem_open("/sentSem1", O_CREAT, S_IRUSR | S_IWUSR, 0);

    pipe(pipefd);

    pid_t player1PID = fork();
    if (player1PID == 0) {
        signal(SIGUSR1, signalHandler);
        pause();
        player(0);
        return 0;
    }

    pid_t player2PID = fork();
    if (player2PID == 0) {
        signal(SIGUSR1, signalHandler);
        pause();
        player(1);
        return 0;
    }

    kill(player1PID, SIGUSR1);
    sleep(1);
    kill(player2PID, SIGUSR1);
    sleep(1);

    int round = 1;
    while (round <= MAX_ROUNDS && player1Score < MAX_SCORE && player2Score < MAX_SCORE) {
        printf("\nRound %d\n", round);
        struct Throw throw1, throw2;

        sem_post(startSem[0]);
        sem_post(startSem[1]);

        sem_post(sendSem[0]);
        sem_wait(sentSem[0]);
        read(pipefd[0], &throw1, sizeof(struct Throw));

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

        sem_post(sendSem[1]);
        sem_wait(sentSem[1]);
        read(pipefd[0], &throw2, sizeof(struct Throw));

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

    close(pipefd[0]);
    close(pipefd[1]);

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
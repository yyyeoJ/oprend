#include "sys/types.h"
#include "unistd.h"
#include "stdlib.h"
#include "signal.h"
#include "stdio.h"
#include "string.h"
#include "time.h"
#include "wait.h"
#include "sys/ipc.h"
#include "sys/msg.h"
#include "sys/shm.h"
#include "sys/sem.h"
#include "sys/stat.h"

#define Alarcot_FEL SIGUSR1

typedef struct
{
    char *question;
    int answer;
} Question;

struct Message
{
    long mtype;
    char mguess;
};

struct sharedData
{
    int firstPlayerAnswer;
    int secPlayerAnswer;
};

pid_t mainProcessValue = 0;
int ready = 0;
int messageQueue;
int semid;
struct sharedData *s;

int semaphoreCreation(const char *pathname, int semaphoreValue)
{
    int semid;
    key_t key;

    key = ftok(pathname, 1);
    if ((semid = semget(key, 1, IPC_CREAT | S_IRUSR | S_IWUSR)) < 0)
        perror("semget");
    if (semctl(semid, 0, SETVAL, semaphoreValue) < 0)
        perror("semctl");

    return semid;
}

void semaphoreOperation(int semid, int op)
{
    struct sembuf operation;

    operation.sem_num = 0;
    operation.sem_op = op;
    operation.sem_flg = 0;

    if (semop(semid, &operation, 1) < 0)
        perror("semop");
}

void semaphoreDelete(int semid)
{
    semctl(semid, 0, IPC_RMID);
}

void starthandler(int sig)
{
    if (sig == Alarcot_FEL)
    {
        ready++;
    }
}

void generateQuestions(Question *questionArray[], const char *questionSentences[], int arraySize)
{
    for (int i = 0; i < arraySize; i++)
    {
        srand(time(NULL));
        int randomAnswer = rand() % 5 + 1;
        questionArray[i] = malloc(sizeof(Question));
        questionArray[i]->question = strdup(questionSentences[i]);
        questionArray[i]->answer = randomAnswer;
    }
}

char *evaluate(int playerAnswer, int goodAnswer)
{
    char *result = malloc(2 * sizeof(char));
    if (playerAnswer == goodAnswer)
    {
        printf("Valaszod: %u, Mikulast kapsz!\n", playerAnswer);
        result[0] = 'j';
        result[1] = '\0';
    }
    else
    {
        printf("Valszod: %u, Virgacsot kapsz!\n", playerAnswer);
        result[0] = 'h';
        result[1] = '\0';
    }
    return result;
}

int rand_id(int max)
{
    return rand() % max;
}

pid_t firstPlayer(int pipe_id_rec, int pipe_id_send)
{
    pid_t process = fork();
    if (process == -1)
    {
        exit(-1);
    }
    if (process > 0)
    {
        return process;
    }

    kill(mainProcessValue, Alarcot_FEL);

    char randomNames[][10] = {"Tigris", "Oroszlan", "Leopard"};

    srand(getpid());
    int r = rand() % 3;
    write(pipe_id_send, &randomNames[r], 10);

    char question[50];
    read(pipe_id_rec, &question, sizeof(question));

    int answer = rand() % 5 + 1;
    printf("Elso jatekos: Kapott kerdes %s es erre a valaszom: %i\n", question, answer);
    write(pipe_id_send, &answer, sizeof(answer));

    semaphoreOperation(semid, -1);
    s->firstPlayerAnswer = answer;
    semaphoreOperation(semid, 1);

    shmdt(s);
    exit(0);
}

pid_t secPlayer(int pipe_id_rec, int pipe_id_send)
{
    pid_t process = fork();
    if (process == -1)
    {
        exit(-1);
    }
    if (process > 0)
    {
        return process;
    }

    kill(mainProcessValue, Alarcot_FEL);

    char randomNames[][10] = {"Malac", "Szamar", "Zebra"};

    srand(getpid());
    int r = rand() % 3;
    write(pipe_id_send, &randomNames[r], 10);

    char question[50];
    read(pipe_id_rec, &question, sizeof(question));

    int answer = rand() % 5 + 1;
    printf("Maosdik jatekos: Kapott kerdes %s es erre a valaszom %i\n", question, answer);
    write(pipe_id_send, &answer, sizeof(answer));

    char puffer;
    puffer = rand_id(100) < 50 ? 'h' : 'j';
    int status;
    struct Message ms = {5, puffer};
    status = msgsnd(messageQueue, &ms, sizeof(char), 0);
    if (status < 0)
    {
        perror("msgsnd");
    }

    semaphoreOperation(semid, -1);
    s->secPlayerAnswer = answer;
    semaphoreOperation(semid, 1);

    shmdt(s);
    exit(0);
}

int main(int argc, char **argv)
{
    mainProcessValue = getpid();
    signal(Alarcot_FEL, starthandler);

    int status;
    key_t mainKey;

    mainKey = ftok(argv[0], 1);
    messageQueue = msgget(mainKey, 0600 | IPC_CREAT);
    if (messageQueue < 0)
    {
        perror("msgget");
        return -1;
    }

    int sh_mem_id;
    sh_mem_id = shmget(mainKey, sizeof(s), IPC_CREAT | S_IRUSR | S_IWUSR);
    s = shmat(sh_mem_id, NULL, 0);

    semid = semaphoreCreation(argv[0], 1);

    int io_pipes[2];
    int succ = pipe(io_pipes);
    if (succ == -1)
    {
        exit(-1);
    }

    int io_pipes1[2];
    int succ1 = pipe(io_pipes1);
    if (succ1 == -1)
    {
        exit(-1);
    }

    int io_pipes2[2];
    int succ2 = pipe(io_pipes2);
    if (succ2 == -1)
    {
        exit(-1);
    }

    int io_pipes3[2];
    int succ3 = pipe(io_pipes3);
    if (succ3 == -1)
    {
        exit(-1);
    }

    pid_t child1_pid = firstPlayer(io_pipes[0], io_pipes1[1]);
    pid_t child2_pid = secPlayer(io_pipes2[0], io_pipes3[1]);

    while (ready < 1)
        ;
    puts("Alarc fenn! - 1");
    while (ready < 2)
        ;
    puts("Alarc fenn! - 2");

    char firstPlayerName[10];
    read(io_pipes1[0], &firstPlayerName, sizeof(firstPlayerName));
    printf("Jatekvezeto hangosan mondja: %s\n", firstPlayerName);

    char secPlayerName[10];
    read(io_pipes3[0], &secPlayerName, sizeof(secPlayerName));
    printf("Jatekvezeto hangosan mondja: %s\n", secPlayerName);

    const char *questionSentences[] = {
        "Question sentence 1",
        "Question sentence 2",
        "Question sentence 3",
    };

    int N = 3;
    Question *questions[N];
    generateQuestions(questions, questionSentences, N);

    srand(time(NULL));
    int randomQuestion = rand() % 3;
    printf("Kerdes adatok: %s, %i, %li\n", questions[randomQuestion]->question, questions[randomQuestion]->answer, strlen(questions[randomQuestion]->question));
    write(io_pipes[1], questions[randomQuestion]->question, strlen(questions[randomQuestion]->question));
    write(io_pipes2[1], questions[randomQuestion]->question, strlen(questions[randomQuestion]->question));

    int firstAnswer;
    int secondAnswer;
    char *firstResult;
    char *secResult;

    read(io_pipes1[0], &firstAnswer, sizeof(int));
    printf("Elso jatekos valsza: %i\n", firstAnswer);
    firstResult = evaluate(firstAnswer, questions[randomQuestion]->answer);
    read(io_pipes3[0], &secondAnswer, sizeof(int));
    printf("Masodik jatekos valsza: %i\n", secondAnswer);
    secResult = evaluate(secondAnswer, questions[randomQuestion]->answer);

    printf("Elso jatekos valasza ismetelen a kovetkezo: (jo / hamis) -> %s\n", firstResult);
    //evaluate(firstAnswer, questions[randomQuestion]->answer);
    char *guessValue = malloc(2 * sizeof(char));
    struct Message ms;
    status = msgrcv(messageQueue, &ms, sizeof(char), 5, 0);
    if (status < 0)
    {
        perror("msgrcv");
    }
    else
    {
        guessValue[0] = ms.mguess;
        guessValue[1] = '\0';
        printf("A kapott tipp a MASODIK jatekostol az eslore: %c\n", ms.mguess);
    }

    if (strcmp(firstResult, guessValue) == 0)
    {
        printf("A masodik jatekos sikeres valaszt adott! Jo valasz, mikulast kap!\n");
    }
    else
    {
        printf("A masodik jatekos nem jo valaszt adott! Rossz valasz, virgacsot kap!\n");
    }

    semaphoreOperation(semid, -1);
    printf("Elso jatekos leadott valasza: %i\n", s->firstPlayerAnswer);
    printf("Masodik jatekos leadott valasza: %i\n", s->secPlayerAnswer);
    semaphoreOperation(semid, 1);
    shmdt(s);

    waitpid(child1_pid, &status, 0);
    printf("Elso jatekos - terminated with status: %d\n", status);
    waitpid(child2_pid, &status, 0);
    printf("Masodik jatekos - terminated with status: %d\n", status);

    for (int i = 0; i < N; i++)
    {
        free(questions[i]->question);
        free(questions[i]);
    }
    free(firstResult);
    free(secResult);
    free(guessValue);

    close(io_pipes[0]);
    close(io_pipes[1]);
    close(io_pipes1[0]);
    close(io_pipes1[1]);
    close(io_pipes2[0]);
    close(io_pipes2[1]);
    close(io_pipes3[0]);
    close(io_pipes3[1]);
    status = msgctl(messageQueue, IPC_RMID, NULL);
    if (status < 0)
    {
        perror("msgctl");
    }
    status = shmctl(sh_mem_id, IPC_RMID, NULL);
    if (status < 0)
    {
        perror("shmctl");
    }
    semaphoreDelete(semid);
    return 0;
}
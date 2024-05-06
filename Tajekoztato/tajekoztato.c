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

// Define a structure for messages
struct message
{
    long mtype; // Message type
    char mtext[1024]; // Message text
};

// Define a structure for shared data
struct sharedData
{
    char text[1024]; // Shared text data
};

// Declare global variables
pid_t mainProcessValue = 0; // Process ID of the main process
int ready = 0; // Flag to indicate readiness
int messageQueue; // Message queue identifier
int semid; // Semaphore identifier
struct sharedData *s; // Pointer to shared data

// Function to create a semaphore
int semaphoreCreation(const char *pathname, int semaphoreValue)
{
    int semid;
    key_t key;

    // Generate a key based on the pathname and an identifier
    key = ftok(pathname, 1);
    // Create a semaphore set and get its ID
    if ((semid = semget(key, 1, IPC_CREAT | S_IRUSR | S_IWUSR)) < 0)
        perror("semget");
    // Set the value of the semaphore set
    if (semctl(semid, 0, SETVAL, semaphoreValue) < 0)
        perror("semctl");

    return semid;
}

// Function to perform semaphore operations
void semaphoreOperation(int semid, int op)
{
    struct sembuf operation;

    // Set semaphore operation parameters
    operation.sem_num = 0; // Index of semaphore in the set
    operation.sem_op = op; // Operation to perform
    operation.sem_flg = 0; // Flags

    // Perform semaphore operation
    if (semop(semid, &operation, 1) < 0)
        perror("semop");
}

// Function to delete a semaphore
void semaphoreDelete(int semid)
{
    // Delete the semaphore set
    semctl(semid, 0, IPC_RMID);
}

// Signal handler function
void readyHandler(int sig)
{
    // Increment the ready counter when SIGUSR1 signal is received
    if (sig == SIGUSR1)
    {
        ready++;
    }
}

// Function for the expert process
pid_t expert(int pipe_id_rec, int pipe_id_send) 
{
    pid_t process = fork(); // Fork a new process
    if (process == -1) // Check for fork failure
        exit(-1);
    if (process > 0) // If in parent process
    {
        return process; // Return to parent process
    }

    kill(mainProcessValue, SIGUSR1); // Send SIGUSR1 signal to the main process

    char puffer[27];
    read(pipe_id_rec, puffer, sizeof(puffer)); // Read from the receive pipe
    printf("expert - Received Question: %s\n", puffer); // Print the received question
    write(pipe_id_send, "Yes", 5); // Write response to the send pipe

    exit(0); // Exit child process
}

// Function for the spokesman process
pid_t spokesman() 
{
    pid_t process = fork(); // Fork a new process
    if (process == -1) // Check for fork failure
        exit(-1);
    if (process > 0) // If in parent process
    {
        return process; // Return to parent process
    }

    kill(mainProcessValue, SIGUSR1); // Send SIGUSR1 signal to the main process
  
    int status;
    struct message ms = {5, "Yes, we have measurements that we will publish."}; // Define a message
    status = msgsnd(messageQueue, &ms, strlen(ms.mtext) + 1, 0); // Send message to message queue
    if (status < 0) // Check for message sending failure
    {
        perror("msgsnd");
    }

    char newData[50] = "20%"; // New data
    semaphoreOperation(semid, -1); // Perform semaphore wait operation
    strcpy(s->text, newData); // Copy data to shared memory
    semaphoreOperation(semid, 1); // Perform semaphore signal operation
    shmdt(s); // Detach shared memory

    exit(0); // Exit child process
}

// Main function
int main(int argc, char **argv)
{
    int status;
    key_t mainKey;
    mainProcessValue = getpid(); // Get process ID of the main process
    signal(SIGUSR1, readyHandler); // Register signal handler for SIGUSR1 signal

    mainKey = ftok(argv[0], 1); // Generate key based on the program name and an identifier
    // Create a message queue
    messageQueue = msgget(mainKey, 0600 | IPC_CREAT);
    if (messageQueue < 0) // Check for message queue creation failure
    {
        perror("msgget");
        return 1;
    }

    int sh_mem_id;
    sh_mem_id = shmget(mainKey, sizeof(s), IPC_CREAT | S_IRUSR | S_IWUSR); // Create shared memory
    s = shmat(sh_mem_id, NULL, 0); // Attach shared memory

    semid = semaphoreCreation(argv[0], 1); // Create semaphore

    int io_pipes[2]; // Array for I/O pipes
    int succ = pipe(io_pipes); // Create pipe
    if (succ == -1) // Check for pipe creation failure
        exit(-1);

    int io_pipes1[2]; // Another array for I/O pipes
    int succ1 = pipe(io_pipes1); // Create another pipe
    if (succ1 == -1) // Check for pipe creation failure
        exit(-1);

    pid_t child1_pid = expert(io_pipes1[0], io_pipes[1]); // Create expert process
    pid_t child2_pid = spokesman(); // Create spokesman process

    // Wait until both child processes are ready
    while (ready < 1)
        ;
    puts("Expert is ready!");
    while (ready < 2)
        ;
    puts("Spokesman is ready!");

    char puffer[5];
    write(io_pipes1[1], "Do we have all documents?", 27); // Write question to expert
    read(io_pipes[0], puffer, sizeof(puffer)); // Read response from expert
    printf("Expert's response: %s\n", puffer); // Print expert's response

    struct message ms;
    status = msgrcv(messageQueue, &ms, 1024, 5, 0); // Receive message from message queue
    if (status < 0) // Check for message receiving failure
    {
        perror("msgrcv");
    }
    else
    {
        printf("Received message from spokesman - type: %ld, text:  %s \n", ms.mtype, ms.mtext);
    }

    semaphoreOperation(semid, -1); // Perform semaphore wait operation
    printf("Spokesman's shared data: %s\n", s->text); // Print shared data
    semaphoreOperation(semid, 1); // Perform semaphore signal operation
    shmdt(s); // Detach shared memory

    // Wait for child processes to terminate
    waitpid(child1_pid, &status, 0);
    printf("Expert terminated with status: %d\n", status);
    waitpid(child2_pid, &status, 0);
    printf("Spokesman terminated with status: %d\n", status);

    // Close pipes
    close(io_pipes1[0]);
    close(io_pipes1[1]);
    close(io_pipes[0]);
    close(io_pipes[1]);
    // Remove message queue
    status = msgctl(messageQueue, IPC_RMID, NULL);
    if (status < 0) // Check for message queue removal failure
    {
        perror("msgctl");
    }
    // Remove shared memory
    status = shmctl(sh_mem_id, IPC_RMID, NULL);
    if (status < 0) // Check for shared memory removal failure
    {
        perror("shmctl");
    }
    semaphoreDelete(semid); // Delete semaphore
    return 0;
}

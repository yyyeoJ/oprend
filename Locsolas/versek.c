#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

// Globális változók
#define MAX_VERS_HOSSZ 255  // Egy vers maximum hossza
#define FILE_PATH "versek.dat"  // Fájl ahol a verseket tároljuk
#define NEW_FILE_PATH "szabadversek.dat"
pid_t mainProcessValue = 0; 
int ready = 0; 
int messageQueue;

struct message
{
    long mtype;
    char mtext[1024];
};

// Prototípusok
void szabadVersek(const char *forras, const char *cel);
void hozzaadas(char *versSzoveg);
void listazas();
void torles(int id);
void modosit(int id, char *ujSzoveg);
void getTwoRandomPoems(char *vers1, char *vers2);
void removeStringFromFile(char* file_path, char* vers);
void locsolas(char *program_name);

// Szignálkezelő függvény
void readyHandler(int sig);

// Gyerekfolyamatok
pid_t child1(int pipe_id_rec);
pid_t child2(int pipe_id_rec);
pid_t child3(int pipe_id_rec);
pid_t child4(int pipe_id_rec);

// Főprogram
int main(int argc, char **argv) {

    int valasztas;
    char vers[MAX_VERS_HOSSZ];
    int id;
    char ujVers[MAX_VERS_HOSSZ];

    szabadVersek(FILE_PATH, NEW_FILE_PATH);

    do {

        printf("X----------------------X\n");
        printf("| 1. Uj vers felvetele |\n");
        printf("| 2. Versek listazasa  |\n");
        printf("| 3. Vers torles       |\n");
        printf("| 4. Vers modositasa   |\n");
        printf("| 5. Locsolas          |\n");
        printf("| 0. Kilepes           |\n");
        printf("X----------------------X\n");
        printf("Valassz: ");
        
        scanf("%d", &valasztas);
        getchar(); // input buffer törlése

        switch (valasztas) {
            case 1:
                printf("Add meg az uj verset: ");
                fgets(vers, sizeof(vers), stdin);
                hozzaadas(vers);
                break;
            case 2:
                listazas();
                break;
            case 3:
                listazas();
                printf("Add meg a torlendo vers azonositojat: ");
                scanf("%d", &id);
                torles(id);
                break;
            case 4:
                printf("Add meg a modositando vers azonositojat: ");
                scanf("%d", &id);
                getchar(); // input buffer törlése
                printf("Add meg az uj tartalmat: ");
                fgets(ujVers, sizeof(ujVers), stdin);  
                modosit(id, ujVers);
                break;
            case 5:
                locsolas(argv[0]);
                break;
            case 0:
                printf("Kilepes...\n");
                break;
            default:
                printf("Ervenytelen valasztas.\n");
        }
        
    } while (valasztas != 0);

    return 0;
}

void hozzaadas(char *versSzoveg) {

    FILE *file = fopen(FILE_PATH, "a"); // append mode

    if (file != NULL) {

        fprintf(file, "%s", versSzoveg);
        fclose(file);
        printf("Vers hozzaadva.\n");

    } else { printf("Hiba: Nem sikerult megnyitni a fajlt.\n"); }
}

void listazas() {

    FILE *file = fopen(FILE_PATH, "r"); // read mode

    if (file != NULL) {
        
        int id = 1;  // Versek kilistázása hozzáadott id-vel
        char line[MAX_VERS_HOSSZ];  // Buffer amibe a verset tároljuk
        
        // Ha nincsenek versek
        if (fgets(line, sizeof(line), file) == NULL) {

            printf("Nincs egyetlen vers sem feljegyezve meg\n");
            fclose(file);
            return;
        // Ha vannak versek: első vers kiírása
        } else {
            printf("Versek:\n");
            printf("%d: %s", id, line);
            id++;
        }
        // Maradék versek kiírása
        while (fgets(line, sizeof(line), file) != NULL) {
            printf("%d: %s", id, line);
            id++;
        }

        fclose(file);

    } else { printf("Hiba: Nem sikerult megnyitni a fajlt.\n"); }
}

void torles(int id) {

    FILE *file = fopen(FILE_PATH, "r");
    FILE *tempFile = fopen("temp.txt", "w");

    if (file != NULL && tempFile != NULL) {

        int currentId = 1;
        char line[MAX_VERS_HOSSZ];

        while (fgets(line, sizeof(line), file) != NULL) {
            if (currentId != id) { fprintf(tempFile, "%s", line); }
            currentId++;
        }

        fclose(file);
        fclose(tempFile);
        remove(FILE_PATH);
        rename("temp.txt", FILE_PATH);
        printf("Vers torolve.\n");

    } else { printf("Hiba: Nem sikerult megnyitni a fajlokat.\n"); }
}

//ugyanúgy működik mint a törlés
void modosit(int id, char *ujSzoveg) {

    FILE *file = fopen(FILE_PATH, "r");
    FILE *tempFile = fopen("temp.txt", "w");  

    if (file != NULL && tempFile != NULL) {

        int currentId = 1;
        char line[MAX_VERS_HOSSZ];

        while (fgets(line, sizeof(line), file) != NULL) {
            // Módosított vers beírása az új fájlba
            if (currentId == id) { fprintf(tempFile, "%s", ujSzoveg);}
            else { fprintf(tempFile, "%s", line); }
            currentId++;
        }

        fclose(file);
        fclose(tempFile);
        remove(FILE_PATH);
        rename("temp.txt", FILE_PATH);
        printf("Vers modositva.\n");
    } else { printf("Hiba: Nem sikerult megnyitni a fajlokat.\n"); }
}

void locsolas(char *program_name){

    int status;
    key_t mainKey;
    mainProcessValue = getpid();
    signal(SIGUSR1, readyHandler);

    mainKey = ftok(program_name, 1);
    // Message queue létrehozása
    messageQueue = msgget(mainKey, 0600 | IPC_CREAT);
    if (messageQueue < 0)
    {
        perror("msgget");
    }

    //Pipe a versek küldéséhez
    int io_pipe[2];
    int succ1 = pipe(io_pipe);
    if (succ1 == -1)
        exit(-1);


    srand(time(NULL));
    int randomNumber = rand() % 4 + 1;

    printf("\nVálasztott gyerek: %d\n",randomNumber);

    pid_t child_pid;
    switch (randomNumber)
    {
    case 1:
        child_pid = child1(io_pipe[0]);
        break;
    case 2:
        child_pid = child2(io_pipe[0]);
        break;
    case 3:
        child_pid = child3(io_pipe[0]);
        break;
    case 4:
        child_pid = child4(io_pipe[0]);
        break;
    }

    
    
    // Várjunk amíg a gyerek meg nem érkezik
    while (ready < 1);

    puts("A gyerek megérkezett");


    char vers1[MAX_VERS_HOSSZ];
    char vers2[MAX_VERS_HOSSZ];
    getTwoRandomPoems(vers1, vers2);


    write(io_pipe[1], vers1, MAX_VERS_HOSSZ); // Vers küldése pipe-on
    write(io_pipe[1], vers2, MAX_VERS_HOSSZ); // Vers küldése pipe-on

    if (strcmp(vers1, " ") != 0){

        struct message ms;
        status = msgrcv(messageQueue, &ms, 1024, 5, 0); // Válaszott vers fogadása üzenetben
        if (status < 0)
        {
            perror("msgrcv");
        }
        else
        {
            removeStringFromFile(NEW_FILE_PATH,ms.mtext);
        }

    }

    

    // Gyerek terminálás megvárása
    waitpid(child_pid, &status, 0);
    printf("\nChild terminated with status: %d\n", status);

    // Pipe-ok bezárása
    close(io_pipe[0]);
    close(io_pipe[1]);
    // Message queue törlése
    status = msgctl(messageQueue, IPC_RMID, NULL);
    if (status < 0)
    {
        perror("msgctl");
    }

}

void readyHandler(int sig)
{
    if (sig == SIGUSR1)
    {
        ready++;
    }
}

void szabadVersek(const char *forras, const char *cel) {
    FILE *forrasFile = fopen(forras, "r");
    FILE *celFile = fopen(cel, "w");

    if (forrasFile == NULL || celFile == NULL) {
        printf("Hiba: Nem sikerult megnyitni a fajlokat.\n");
        return;
    }

    char sor[MAX_VERS_HOSSZ];

    while (fgets(sor, sizeof(sor), forrasFile) != NULL) {
        fputs(sor, celFile);
    }

    fclose(forrasFile);
    fclose(celFile);
}

void getTwoRandomPoems(char *vers1, char *vers2) {
    FILE *file = fopen(NEW_FILE_PATH, "r");

    if (file != NULL) {
        int poemCount = 0;
        char line[MAX_VERS_HOSSZ];
        while (fgets(line, sizeof(line), file) != NULL) {
            if (strcmp(line, "\n") != 0) {
                poemCount++;
            }
        }

        if (poemCount < 1) {
            printf("Minden vers fel lett hasznalva :( \n");
            strcpy(vers1, " ");
            strcpy(vers2, " ");
            fclose(file);
            return;
        }

        rewind(file);

        srand(time(NULL));
        int randomLine1 = rand() % poemCount;
        int randomLine2 = -1;

        if (poemCount > 1) {
            do {
                randomLine2 = rand() % poemCount;
            } while (randomLine2 == randomLine1);
        }

        int currentLine = 0;
        while (fgets(line, sizeof(line), file) != NULL) {
            if (strcmp(line, "\n") != 0) {
                if (currentLine == randomLine1) {
                    strcpy(vers1, line);
                } else if (currentLine == randomLine2) {
                    strcpy(vers2, line);
                }
                currentLine++;
            }
        }

        if (poemCount == 1) {
            strcpy(vers2, " ");
        }

        fclose(file);
    } else {
        printf("Hiba: Nem sikerult megnyitni a fajlt.\n");
    }
}

void removeStringFromFile(char* file_path, char* vers) {
    FILE *file = fopen(file_path, "r");  
    FILE *tempFile = fopen("temp.txt", "w");
    if (file != NULL && tempFile != NULL) {
        char line[MAX_VERS_HOSSZ];

        while (fgets(line, sizeof(line), file) != NULL) {
            if (strcmp(line, vers) != 0) {
                fprintf(tempFile, "%s", line);
            }

        }

        fclose(file);
        fclose(tempFile);
        
        remove(file_path);
        rename("temp.txt", file_path);

    } else { 
        printf("Hiba: Nem sikerült megnyitni a fájlokat.\n"); 
    }
}

pid_t child1(int pipe_id_rec)
{
    pid_t process = fork();
    if (process == -1)
        exit(-1);
    if (process > 0)
    {
        return process;
    }

    printf("Signal -> Szülő\n");
    kill(mainProcessValue, SIGUSR1); // Signal küldése szülőnek

    // Versek fogadása pipe-on
    char vers1[MAX_VERS_HOSSZ];
    char vers2[MAX_VERS_HOSSZ];

    read(pipe_id_rec, vers1, sizeof(vers1));
    if (strcmp(vers1, " ") != 0){
    printf("\nPipe-on kapott vers 1: %s", vers1);
    }

    read(pipe_id_rec, vers2, sizeof(vers2));
    if (strcmp(vers2, " ") != 0){
    printf("Pipe-on kapott vers 2: %s\n", vers2);
    }

    if(strcmp(vers1, " ") != 0){
        // Random vers választása a 2-ből
        srand(time(NULL));
        int randomNumber = 1; 

        if (strcmp(vers2, " ") != 0) { 
            randomNumber = rand() % 2 + 1;
        }

        // Kiválasztott vers visszaküldése message queue-ban
        
        int status;
        struct message ms;
        ms.mtype = 5;

        if(randomNumber == 1) {
            // 1. vers visszaküldése
            strcpy(ms.mtext, vers1);
        } else if(randomNumber == 2) {
            // 2. vers visszaküldése, csak ha nem üres
            if (strcmp(vers2, " ") != 0) {
                strcpy(ms.mtext, vers2);
            } else {
                randomNumber = 1;
                strcpy(ms.mtext, vers1);
            }
        }

        status = msgsnd(messageQueue, &ms, strlen(ms.mtext) + 1, 0);
        if (status < 0) {
            perror("msgsnd");
        }

        printf("Választott vers: %d\n", randomNumber);
        printf("%sSzabad-e locsolni!\n", ms.mtext);
    }

    exit(0);
}

pid_t child2(int pipe_id_rec)
{
    pid_t process = fork();
    if (process == -1)
        exit(-1);
    if (process > 0)
    {
        return process;
    }

    printf("Signal -> Szülő\n");
    kill(mainProcessValue, SIGUSR1); // Signal küldése szülőnek

    // Versek fogadása pipe-on
    char vers1[MAX_VERS_HOSSZ];
    char vers2[MAX_VERS_HOSSZ];

    read(pipe_id_rec, vers1, sizeof(vers1));
    if (strcmp(vers1, " ") != 0){
    printf("\nPipe-on kapott vers 1: %s", vers1);
    }

    read(pipe_id_rec, vers2, sizeof(vers2));
    if (strcmp(vers2, " ") != 0){
    printf("Pipe-on kapott vers 2: %s\n", vers2);
    }

    if(strcmp(vers1, " ") != 0){
        // Random vers választása a 2-ből
        srand(time(NULL));
        int randomNumber = 1; 

        if (strcmp(vers2, " ") != 0) { 
            randomNumber = rand() % 2 + 1;
        }

        // Kiválasztott vers visszaküldése message queue-ban
        
        int status;
        struct message ms;
        ms.mtype = 5;

        if(randomNumber == 1) {
            // 1. vers visszaküldése
            strcpy(ms.mtext, vers1);
        } else if(randomNumber == 2) {
            // 2. vers visszaküldése, csak ha nem üres
            if (strcmp(vers2, " ") != 0) {
                strcpy(ms.mtext, vers2);
            } else {
                randomNumber = 1;
                strcpy(ms.mtext, vers1);
            }
        }

        status = msgsnd(messageQueue, &ms, strlen(ms.mtext) + 1, 0);
        if (status < 0) {
            perror("msgsnd");
        }

        printf("Választott vers: %d\n", randomNumber);
        printf("%sSzabad-e locsolni!\n", ms.mtext);
    }

    exit(0);
}

pid_t child3(int pipe_id_rec)
{
    pid_t process = fork();
    if (process == -1)
        exit(-1);
    if (process > 0)
    {
        return process;
    }

    printf("Signal -> Szülő\n");
    kill(mainProcessValue, SIGUSR1); // Signal küldése szülőnek

    // Versek fogadása pipe-on
    char vers1[MAX_VERS_HOSSZ];
    char vers2[MAX_VERS_HOSSZ];

    read(pipe_id_rec, vers1, sizeof(vers1));
    if (strcmp(vers1, " ") != 0){
    printf("\nPipe-on kapott vers 1: %s", vers1);
    }

    read(pipe_id_rec, vers2, sizeof(vers2));
    if (strcmp(vers2, " ") != 0){
    printf("Pipe-on kapott vers 2: %s\n", vers2);
    }

    if(strcmp(vers1, " ") != 0){
        // Random vers választása a 2-ből
        srand(time(NULL));
        int randomNumber = 1; 

        if (strcmp(vers2, " ") != 0) { 
            randomNumber = rand() % 2 + 1;
        }

        // Kiválasztott vers visszaküldése message queue-ban
        
        int status;
        struct message ms;
        ms.mtype = 5;

        if(randomNumber == 1) {
            // 1. vers visszaküldése
            strcpy(ms.mtext, vers1);
        } else if(randomNumber == 2) {
            // 2. vers visszaküldése, csak ha nem üres
            if (strcmp(vers2, " ") != 0) {
                strcpy(ms.mtext, vers2);
            } else {
                randomNumber = 1;
                strcpy(ms.mtext, vers1);
            }
        }

        status = msgsnd(messageQueue, &ms, strlen(ms.mtext) + 1, 0);
        if (status < 0) {
            perror("msgsnd");
        }

        printf("Választott vers: %d\n", randomNumber);
        printf("%sSzabad-e locsolni!\n", ms.mtext);
    }

    exit(0);
}

pid_t child4(int pipe_id_rec)
{
    pid_t process = fork();
    if (process == -1)
        exit(-1);
    if (process > 0)
    {
        return process;
    }

    printf("Signal -> Szülő\n");
    kill(mainProcessValue, SIGUSR1); // Signal küldése szülőnek

    // Versek fogadása pipe-on
    char vers1[MAX_VERS_HOSSZ];
    char vers2[MAX_VERS_HOSSZ];

    read(pipe_id_rec, vers1, sizeof(vers1));
    if (strcmp(vers1, " ") != 0){
    printf("\nPipe-on kapott vers 1: %s", vers1);
    }

    read(pipe_id_rec, vers2, sizeof(vers2));
    if (strcmp(vers2, " ") != 0){
    printf("Pipe-on kapott vers 2: %s\n", vers2);
    }

    if(strcmp(vers1, " ") != 0){
        // Random vers választása a 2-ből
        srand(time(NULL));
        int randomNumber = 1; 

        if (strcmp(vers2, " ") != 0) { 
            randomNumber = rand() % 2 + 1;
        }

        // Kiválasztott vers visszaküldése message queue-ban
        
        int status;
        struct message ms;
        ms.mtype = 5;

        if(randomNumber == 1) {
            // 1. vers visszaküldése
            strcpy(ms.mtext, vers1);
        } else if(randomNumber == 2) {
            // 2. vers visszaküldése, csak ha nem üres
            if (strcmp(vers2, " ") != 0) {
                strcpy(ms.mtext, vers2);
            } else {
                randomNumber = 1;
                strcpy(ms.mtext, vers1);
            }
        }

        status = msgsnd(messageQueue, &ms, strlen(ms.mtext) + 1, 0);
        if (status < 0) {
            perror("msgsnd");
        }

        printf("Választott vers: %d\n", randomNumber);
        printf("%sSzabad-e locsolni!\n", ms.mtext);
    }

    exit(0);
}
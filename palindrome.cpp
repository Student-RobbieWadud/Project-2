#include <iostream>
#include <unistd.h>
#include <ctime>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/stat.h>
#include <fstream>
#include <cstdlib>
#include <string.h>
#include <stdbool.h>

using namespace std;

struct shared_memory
{
        int count;
        int turn;
        int flags[20];
        char data[20][256];
        int slaveProcessGroup;
};

enum state { idle, want_in, in_cs };

void sigHandler(int);

int id;

int main(int argc, char ** argv)
{
        setvbuf(stdout, NULL, _IONBF, 0);

        signal(STGTERM, sigHandler);

        int index;

        if(argc < 2)
        {
                perror("No argument supplied for id");
                exit(1);
        }

        else
        {
                id = atoi(argv[1]);
                index = id - 1;
        }

        srand(time(0) + id);

        int N;

        int shmKey = ftok("makefile", 'p');

        int shmSegmentID;

        shared_memory* shm;

        if((shmSegmentID = shmget(shmKey, sizeof(struct shared_memory), IPC_CREAT | S_IRUSR | S_IWUSR)) < 0)
        {
                perror("shmget: Failed to allocate shared memory");
                exit(1);
        }

        else
        {
                shm = (struct shared_memory*) shmat(shmSegmentID, NULL, 0);
        }

        N = shm->count;

        printf("%s: Process %d Entering critical section\n", getFormattedTime(), id);

        cerr << getFormattedTime() << ": Process " << id << " wants to enter critical section\n";

        int l = 0;

        int r = strlen(shm->data[index]) - 1;

        bool palin = true;

        while(r > l)
        {
                if(tolower(shm->data[index][l]) != tolower(shm->data[index][r]))
                {
                        palin = false;
                        break;
                }

                l++;
                r--;
        }

        int j;

        do
        {
                shm->flags[id - 1] = want_in;
                j = shm->turn;

                while(j != id - 1)
                {
                        j = (shm->flags[j] != idle ? shm->turn : (j + 1) % N;
                }

                shm->flags[id - 1] = in_cs;

                for(j = 0; j < N; j++)
                {
                        if((j != id - 1) && (shm->flags[j] == in_cs))
                        {
                                break;
                        }
                }

        } while((j < N) || ((shm->turn != id - 1) && (shm->flags[shm->turn] != idle)));

        shm->turn = id - 1;

        printf("%s: Process %d in critical section\n", getFormattedTime(), id);

        sleep(rand() % 3);

        FILE *file = fopen(palin ? "palin.out" : "nopalin.out", "a+");

        if(file == NULL)
        {
                perror("");
                exit(1);
        }

        fprintf(file, "%s\n", shm->data[id - 1]);
        fclose(file);

        file = fopen("output.log", "a+");

        if(file == NULL)
        {
                perror("");
                exit(1);
        }

        fprintf(file, "%s %d %d %s\n", getFormattedTime(), getpid(), id - 1, shm->data[id - 1]);
        fclose(file);

        printf("%s: Process %d exiting critical section\n", getFormattedTime(), id);

        j = (shm->turn + 1) % N;

        while(shm->flags[j] == idle)
        {
                j = (j + 1) % N;
        }

        shm->turn = j;

        shm->flags[id - 1] = idle;

        return 0;
}

void sigHandler(int signal)
{
        if(signal == STGTERM)
        {
                printf("In Signal Handler Function\n");
                exit(1);
        }
}

char* getFormattedTime()
{
        int timeStringLength;
        string timeFormat;

        timeStringLength = 9;
        timeFormat = "%H:%M:%S";

        time_t seconds = time(0);

        struct tm * ltime = localtime(&seconds);

        char *timeString = new char[timeStringLength];

        strftime(timeString, timeStringLength, timeFormat.c_str(), ltime);

        return timeString;
}

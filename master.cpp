#include <sys/time.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cstdlib>
#include <string>
#include <cstring>

using namespace std;

struct shared_memory
{
        int count;
        int turn;

        int flags[20];
        char data[20][256];

        int slaveProcessGroup;
};

void trySpawnChild(int count);
void spawn(int count);
void sigHandler(int SIG_CODE);
void timerSignalHandler(int);
void releaseMemory();

void parentInterrupt(int seconds);
void timer(int seconds);

const int MAX_NUM_OF_PROCESSES_IN_SYSTEM = 20;
int currentConcurrentProcessesInSystem = 0;

int maxTotalProcessesInSystem = 4;
int maxConcurrentProcessesInSystem = 2;
int durationBeforeTermination = 100;
int shmKey = ftok("makefile", 'p');
int shmSegmentID;
struct shared_memory* shm;

int status = 0;

int startTime;

int main(int argc, char** argv)
{
        signal(SIGINT, sigHandler);

        if((shmSegmentID = shmget(shmKey, sizeof(struct shared_memory), IPC_CREAT | S_IRUSR | S_IWUSR)) < 0)
        {
                perror("Failed to create");
                exit(1);
        }

        else
        {
                shm = (struct shared_memory*) shmat(shmSegmentID, NULL, 0);
        }

        int c;

        while((c = getopt(argc, argv, "hn:s:t:")) != -1)
        {
                switch(c)
                {
                        case 'h':

                                cout << "USAGE: " << endl;
                                cout << "./master -h for help command" << endl;
                                cout << "./master [-n x] [-s x] [-t time] infile" << endl;
                                cout << "PARAMETERS: " endl;
                                cout << "-h Describes how the project should be run and then terminate." << endl;
                                cout << "-n x Indicate the maximum total of child processes master will ever create. (Default 4)" << endl;
                                cout << "-s x Indicate the number of children allowed to exist in the system at the same time (Default 2)" << endl;
                                cout << "-t time The time in seconds after which the processes will terminate, even if it has not finished (Default 100)" << endl;
                                cout << "infile containing strings to be tested" << endl;
                                exit(0);
                        break;

                        case 'n':

                                maxTotalProcessesInSystem = atoi(optarg);

                                if((maxTotalProcessesInSystem <= 0) || maxTotalProcessesInSystem > MAX_NUM_OF_PROCESSES_IN_SYSTEM)
                                {
                                        perror("Max Processes: Cannot be 0 or greater than 20");
                                        exit(1);
                                }
                        break;

                        case 's':

                                maxConcurrentProcessesInSystem = atoi(optarg);

                                if(maxConcurrentProcessesInSystem <= 0)
                                {
                                        perror("The max concurrent processes CANNOT be less than or equal to 0.");
                                        exit(1);
                                }
                        break;

                        case 't':

                                durationBeforeTermination = atoi(optarg);

                                if(durationBeforeTermination <= 0)
                                {
                                        perror("Master cannot have a duration less than or equal to 0.");
                                        exit(1);
                                }
                        break;

                        default:
                                perror("THESE ARE NOT VALID PARAMETERS: Use -h for help");
                                cout << "./master -h for help command" << endl;
                                cout << "./master [-n x] [-s x] [-t time] infile" << endl;
                                exit(1);
                        break;
                }
        }

        parentInterrupt(durationBeforeTermination);

        int i = 0;
        FILE *fp = fopen(argv[optind], "r");

        if(fp == 0)
        {
                perror("fopen: File not found");
                exit(1);
        }

        char line[256];

        while(fgets(line, sizeof(line), fp) != NULL)
        {
                line[strlen(line) - 1] = '\0';
                strcpy(shm->data[i], line);
                i++;
        }

        int count = 0;

        if(i < maxTotalProcessesInSystem)
        {
                maxTotalProcessesInSystem = i;
        }

        if(maxTotalProcessesInSystem < maxConcurrentProcessesInSystem)
        {
                maxConcurrentProcessesInSystem = maxTotalProcessesInSystem;
        }

        shm->count = maxTotalProcessesInSystem;

        while(count < maxConcurrentProcessesInSystem)
        {
                trySpawnChild(count++);
        }

        while(currentConcurrentProcessesInSystem > 0)
        {
                wait(NULL);
                --currentConcurrentProcessesInSystem;
                trySpawnChild(count++);
        }

        releaseMemory();

        return 0;
}

void trySpawnChild(int count)
{
        if((currentConcurrentProcessesInSystem < maxConcurrentProcessesInSystem) && (count < maxTotalProcessesInSystem))
        {
                spawn(count);
        }
}

void spawn(int count)
{
        ++currentConcurrentProcessesInSystem;
        if(fork() == 0)
        {
                if(count == 1)
                {
                        shm->slaveProcessGroup = getpid();
                }

                setpgid(0, shm->slaveProcessGroup);

                char buf[256];

                sprintf(buf, "%d", count + 1);

                execl("./palin", "palin", buf, (char*) NULL);

                exit(0);
        }
}

void sigHandler(int signal)
{
        killpg(shm->slaveProcessGroup, SIGTERM);
        int status;

        while(wait(&status) > 0)
        {
                if(WIFEXITED(status))
                {
                        printf("Ok: Child exited with exit status: %d\n", WEXITSTATUS(status));
                }

                else
                {
                        printf("ERROR: Child has not terminated correctly\n");
                }
        }

        releaseMemory();

        cout << "Exiting master process" << endl;

        exit(0);
}

void releaseMemory()
{
        shmdt(shm);
        shmctl(shmSegmentID, IPC_RMID, NULL);
}

void parentInterrupt(int seconds)
{
        timer(seconds);

        struct sigaction sa;

        sigemptyset(&sa.sa_mask);

        sa.sa_handler = &sigHandler;

        sa.sa_flags = SA_RESTART;

        if(sigaction(SIGALRM, &sa, NULL) == -1)
        {
                perror("ERROR");
        }
}

void timer(int seconds)
{
        struct itimerval value;

        value.it_value.tv_sec = seconds;

        value.it_value.tv_usec = 0;

        value.it_value.tv_usec = 0;

        value.it_interval.tv_sec = 0;

        value.it_interval.tv_usec = 0;

        if(setitimer(ITIMER_REAL, &value, NULL) == -1)
        {
                perror("ERROR");
        }
}

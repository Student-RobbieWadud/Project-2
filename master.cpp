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
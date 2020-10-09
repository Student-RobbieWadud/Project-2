//Import Statements
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>

using namespace std;

//Global Variables

//The max number of processes as the problem states
const int MAX_PROCESSES = 20;

//Variables with defaults on the project paper
int maxConcurrentProcesses = 2, maxTotalProcesses = 4, terminationTime = 100;

//Variables for the shared memory key and shared memory segment ID
int sharedMemoryKey = ftok("makefile", 'p'), sharedMemorySegmentID;

//Variable to keep track of the current concurrent processes
int concurrentProcesses = 0;

//Do I need this? int status = 0;

//Creating the shared memory as a struct
struct mySharedMemory
{
        int turn, count, slaveGroup, flags[20];
        char data[20][256];
};

//The shared memory pointer
struct mySharedMemory* sharedMemory;

//All the function declarations

//Function countdowns to 0 and resets. Also generates a signal.
void timer(int seconds);

//This function sends a signal used in the timer function
void interrupt(int seconds);

//Function to spawn children
void spawnChildren(int count);

//Function to handle a Ctrl + C interrupt
void signalHandler(int SIGNAL_CODE);

//Function to release all shared memory
void release();

int main(int argc, char** argv)
{
        signal(SIGINT, signalHandler);

        if((sharedMemorySegmentID = shmget(sharedMemoryKey, sizeof(struct mySharedMemory), IPC_CREAT | S_IRUSR | S_IWUSR)) < 0)
        {
                perror("Failed to create");
                exit(1);
        }

        else
        {
                sharedMemory = (struct mySharedMemory*) shmat(sharedMemorySegmentID, NULL, 0);
        }

        int c;

        while((c = getopt(argc, argv, "hn:s:t:")) != -1)
        {
                switch(c)
                {
                        case 'h':

                                cout << "USAGE: " << endl;
                                cout << "Type in ./master -h for assistance" << endl;
                                cout << "This is the format you should type in: ./master [-n x] [-s x] [-t time] yourFile.txt" << endl;
                                cout << "Explanation of the parameters: " << endl;
                                cout << "-h Tells you what the program does." << endl;
                                cout << "-n x is the max total child processes the master will ever create." << endl;
                                cout << "-s x is the number of children able to exist at the same time." << endl;
                                cout << "-t time is the time (in seconds) after which the processes will terminate." << endl;
                                exit(0);
                        break;

                        case 'n':

                                maxTotalProcesses = atoi(optarg);

                                if((maxTotalProcesses <= 0) || maxTotalProcesses > MAX_PROCESSES)
                                {
                                        perror("There can be no more than 20 processes and no less than 0 processes.");
                                        exit(1);
                                }
                        break;

                        case 's':

                                maxConcurrentProcesses = atoi(optarg);

                                if(maxConcurrentProcesses <= 0)
                                {
                                        perror("There must be at least 1 or more max concurrent processes.");
                                        exit(1);
                                }
                        break;

                        case 't':

                                terminationTime = atoi(optarg);

                                if(terminationTime <= 0)
                                {
                                        perror("The master process cannot have a duration less than or equal to 0.");
                                        exit(1);
                                }
                        break;

                        default:
                                perror("YOU HAVE NOT TYPED IN VALID PARAMETERS: Please type in ./master -h for help.");
                                exit(1);
                        break;
                }
        }

        interrupt(terminationTime);

        int i = 0;
        FILE *fp = fopen(argv[optind], "r");

        if(fp == 0)
        {
                perror("File not found!");
                exit(1);
        }

        char line[256];

        while(fgets(line, sizeof(line), fp) != NULL)
        {
                line[strlen(line) - 1] = '\0';
                strcpy(sharedMemory->data[i], line);
                i++;
        }

        int count = 0;

        if(i < maxTotalProcesses)
        {
                maxTotalProcesses = i;
        }

        if(maxTotalProcesses < maxConcurrentProcesses)
        {
                maxConcurrentProcesses = maxTotalProcesses;
        }

        sharedMemory->count = maxTotalProcesses;

        while(count < maxConcurrentProcesses)
        {
                spawnChildren(count++);
        }

        while(concurrentProcesses > 0)
        {
                wait(NULL);
                --concurrentProcesses;
                spawnChildren(count++);
        }

        release();

        return 0;
}

//Function to time
void timer(int seconds)
{
        //Struct to access seconds variables
        struct itimerval myTimer;

        myTimer.it_value.tv_sec = seconds;
        myTimer.it_value.tv_usec = 0;
        myTimer.it_interval.tv_sec = 0;
        myTimer.it_interval.tv_usec = 0;

        if(setitimer(ITIMER_REAL, &myTimer, NULL) == -1)
        {
                perror("There has been an error");
        }
}

//Function that counts down to 0 and then sends a signal
void interrupt(int seconds)
{
        timer(seconds);

        //Struct to use for signals
        struct sigaction signalStuff;

        sigemptyset(&signalStuff.sa_mask);
        signalStuff.sa_handler = &signalHandler;
        signalStuff.sa_flags = SA_RESTART;

        if(sigaction(SIGALRM, &signalStuff, NULL) == -1)
        {
                perror("There has been an error");
        }
}

//Function that spawns children, gives out an ID, and then put the children into palin
void spawnChildren(int count)
{
        if((concurrentProcesses < maxConcurrentProcesses) && (count < maxTotalProcesses))
        {
                ++concurrentProcesses;

                if(fork() == 0)
                {
                        if(count == 1)
                        {
                                sharedMemory->slaveGroup = getpid();
                        }

                        setpgid(0, sharedMemory->slaveGroup);

                        char buffer[256];

                        sprintf(buffer, "%d", count + 1);

                        execl("./palin", "palin", buffer, (char*) NULL);

                        exit(0);

                }
        }
}

//Function to handle a Ctrl + C interrupt
void signalHandler(int signal)
{
        killpg(sharedMemory->slaveGroup, SIGTERM);
        int status;

        while(wait(&status) > 0)
        {
                if(WIFEXITED(status))
                {
                        printf("The child process exited with this exit status: %d", WEXITSTATUS(status));
                }

                else
                {
                        printf("Child has NOT terminated");
                }
        }

        release();

        cout << "Now exiting master process" << endl;

        exit(0);
}

//Function to release the shared memory
void release()
{
        shmdt(sharedMemory);
        shmctl(sharedMemorySegmentID, IPC_RMID, NULL);
}
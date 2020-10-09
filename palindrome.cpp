//Import Statements
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

using namespace std;

//Creating the shared memory as a struct
struct mySharedMemory
{
        int turn, count, flags[20];
        char data[20][256];
};

//From the notes
enum state {idle, want_in, in_cs};

//Function to get the time
char* getTime();

//Function used in case the user presses Ctrl + C
void handlerForControlC(int);

//The universal process ID
int processID;

int main(int argc, char ** argv)
{
        setvbuf(stdout, NULL, _IONBF, 0);

        signal(SIGTERM, handlerForControlC);

        int index;

        if(argc < 2)
        {
                perror("No argument supplied for id");
                exit(1);
        }

        else
        {
                processID = atoi(argv[1]);
                index = processID - 1;
        }

        srand(time(0) + processID);

        //Creating variables for the shared memory key, shared memory segment ID, and slave processes
        int sharedMemoryKey = ftok("makefil", 'p'), sharedMemorySegmentID, numberOfSlaveProcesses;

        //The shared memory pointer
        mySharedMemory* sharedMemory;

        //If statements to allocate shared memory
        if((sharedMemorySegmentID = shmget(sharedMemoryKey, sizeof(struct mySharedMemory), IPC_CREAT | S_IRUSR | S_IWUSR)) < 0)
        {
                perror("Could not allocate shared memory!");
                exit(1);
        }

        else
        {
                sharedMemory = (struct mySharedMemory*) shmat(sharedMemorySegmentID, NULL, 0);
        }

        numberOfSlaveProcesses = sharedMemory->count;

        printf("Process %s is now going in the critical section. Time: %d ", processID, getTime());

        //These variables are for palindrome calculations
        int left = 0, right = strlen(sharedMemory->data[index]) - 1;

        //Flag used when I have a string that is a palindrome
        bool flag = true;

        //While loop to see if I have a palindrome
        while(right > left)
        {
                if(tolower(sharedMemory->data[index][left]) != tolower(sharedMemory->data[index][right]))
                {
                        flag = false;
                        break;
                }

                left++;
                right--;
        }

        //A counter var for the notes algorithm
        int j;

        //Do-while loop for to solve the Critcal Section Problem
        do
        {
                sharedMemory->flags[processID - 1] = want_in;
                j = sharedMemory->turn;

                while(j != processID - 1)
                {
                        j = (sharedMemory->flags[j] != idle) ? sharedMemory->turn : (j + 1) % numberOfSlaveProcesses;
                }

                sharedMemory->flags[processID - 1] = in_cs;

                for(j = 0; j < numberOfSlaveProcesses; j++)
                {
                        if((j != processID - 1) && (sharedMemory->flags[j] == in_cs))
                        {
                                break;
                        }
                }

        } while((j < numberOfSlaveProcesses) || ((sharedMemory->turn != processID - 1) && (sharedMemory->flags[sharedMemory->turn] != idle)));

        sharedMemory->turn = processID - 1;

        printf("Process %s is now going in the critical section. Time: %d", processID, getTime());

        //Do the 0-2 second wait stated in the project
        sleep(rand() % 3);

        //Write palindromes to one file and non-palindromes to another file
        FILE *file = fopen(flag ? "palin.out" : "nopalin.out", "a+");

        if(file == NULL)
        {
                perror("There's no file!");
                exit(1);
        }

        fprintf(file, "%s", sharedMemory->data[processID - 1]);

        fclose(file);

        file = fopen("output.log", "a+");

        if(file == NULL)
        {
                perror("There's no file!");
                exit(1);
        }

        fprintf(file, "%s %d %d %s\n", getTime(), getpid(), processID - 1, sharedMemory->data[processID - 1]);
        fclose(file);

        printf("Process %s is now going in the critical section. Time: %d", processID, getTime());

        j = (sharedMemory->turn + 1) % numberOfSlaveProcesses;

        while(sharedMemory->flags[j] == idle)
        {
                j = (j + 1) % numberOfSlaveProcesses;
        }

        sharedMemory->turn = j;

        sharedMemory->flags[processID - 1] = idle;

        return 0;
}

//Function to get the time of a process ID
char* getTime()
{
        int timeLength = 9;

        char *timeString = new char[timeLength];

        string timeFormat = "%H:%M:%S";

        time_t seconds = time(0);

        struct tm * localTime = localtime(&seconds);

        strftime(timeString, timeLength, timeFormat.c_str(), localTime);

        return timeString;
}

//Function that exits when the user does Ctrl + C
void handlerForControlC(int userSignal)
{
        if(userSignal == SIGTERM)
        {
                exit(1);
        }
}
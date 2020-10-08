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
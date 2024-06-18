#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <asm-generic/signal-defs.h>
#include <bits/sigaction.h>

// Constants for parking capacities
#define TEMP_AUTOMOBILE_CAPACITY 8
#define TEMP_PICKUP_CAPACITY 4

volatile int running = 1;

// Semaphores for synchronization
sem_t newAutomobile;
sem_t inChargeforAutomobile;
sem_t newPickup;
sem_t inChargeforPickup;

// Counters to track available temporary parking spots
int mFree_automobile = TEMP_AUTOMOBILE_CAPACITY;
int mFree_pickup = TEMP_PICKUP_CAPACITY;

// Mutexes to protect counter variables
pthread_mutex_t auto_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pickup_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t entry_lock = PTHREAD_MUTEX_INITIALIZER; // Mutex for entry synchronization

pthread_t pickup_gen_thread, automobile_gen_thread;
pthread_t auto_attendant_thread, pickup_attendant_thread;

// ctrl c signal handler
void intHandler(int dummy)
{
    running = 0;
    sem_post(&newAutomobile);
    sem_post(&newPickup);
    sem_post(&inChargeforAutomobile);
    sem_post(&inChargeforPickup);

    int res = pthread_join(pickup_gen_thread, NULL);
    // printf("res1: %d\n", res);
    res = pthread_join(automobile_gen_thread, NULL);
    // printf("res2: %d\n", res);
    res = pthread_join(auto_attendant_thread, NULL);
    // printf("res3: %d\n", res);
    res = pthread_join(pickup_attendant_thread, NULL);
    // printf("res4: %d\n", res);
    // printf("\033[0;32mSimulation ended**************************.\n");
    // Destroy semaphores
    sem_destroy(&newAutomobile);
    sem_destroy(&inChargeforAutomobile);
    sem_destroy(&newPickup);
    sem_destroy(&inChargeforPickup);
    // printf("\033[0;32mSimulation really ended**************************.\n");
    printf("\033[0;31mSimulation ended by user.\n");
    exit(0);
}

// This function simulates a vehicle owner arriving at the parking lot. It will attempt to park the vehicle in a temporary spot. 
// If there are no free spots, the vehicle owner will leave.
// Mutexes are used to protect the counter variables. A semaphore is used to signal the attendant that a vehicle has arrived.
void *carOwner(void *arg)
{
    char vehicle_type = *(char *)arg;

    pthread_mutex_lock(&entry_lock); // Ensure only one vehicle can enter at a time

    if (vehicle_type == 'A')
    {
        pthread_mutex_lock(&auto_lock);
        if (mFree_automobile > 0)
        {
            mFree_automobile--;
            printf("\033[0;36mAutomobile owner parked in temporary spot. Free automobile spots: %d\n", mFree_automobile);
            sem_post(&newAutomobile);
        }
        else
        {
            printf("\033[0;31mAutomobile owner left, no free temporary spot.\n");
            pthread_mutex_unlock(&auto_lock);
            pthread_mutex_unlock(&entry_lock);
            return NULL;
        }
        pthread_mutex_unlock(&auto_lock);
    }
    else if (vehicle_type == 'P')
    {
        pthread_mutex_lock(&pickup_lock);
        if (mFree_pickup > 0)
        {
            mFree_pickup--;
            printf("\033[0;36mPickup owner parked in temporary spot. Free pickup spots: %d\n", mFree_pickup);
            sem_post(&newPickup);
        }
        else
        {
            printf("\033[0;31mPickup owner left, no free temporary spot.\n");
            pthread_mutex_unlock(&pickup_lock);
            pthread_mutex_unlock(&entry_lock);
            return NULL;
        }
        pthread_mutex_unlock(&pickup_lock);
    }

    pthread_mutex_unlock(&entry_lock); // Release the entry lock
    return NULL;
}

// This function simulates a car attendant managing the parking lot. The attendant will park vehicles in the temporary spots.
// The attendant will wait for a signal from the vehicle owner that a vehicle has arrived.
// The attendant will then park the vehicle and signal the vehicle owner that the vehicle has been parked.
// The attendant will then wait for the next vehicle to arrive.
// Mutexes are used to protect the counter variables. Semaphores are used to signal the attendant that a vehicle has arrived.
void *carAttendant()
{

    while (running)
    {
        if (sem_trywait(&inChargeforAutomobile) == 0)
        {
            sem_wait(&newAutomobile);
            // Simulate the time taken to park the automobile
            sleep(1);
            pthread_mutex_lock(&auto_lock);
            mFree_automobile++;
            printf("\033[0;35mAttendant parked automobile. Free automobile spots: %d\n", mFree_automobile);
            pthread_mutex_unlock(&auto_lock);
            sem_post(&inChargeforAutomobile);
        }
        else if (sem_trywait(&inChargeforPickup) == 0)
        {
            sem_wait(&newPickup);
            // Simulate the time taken to park the pickup
            sleep(1);
            pthread_mutex_lock(&pickup_lock);
            mFree_pickup++;
            printf("\033[0;35mAttendant parked pickup. Free pickup spots: %d\n", mFree_pickup);
            pthread_mutex_unlock(&pickup_lock);
            sem_post(&inChargeforPickup);
        }
    }
    return NULL;
}

// This function simulates a vehicle generator that generates vehicles at random intervals.
void *pickup_generator(void *arg)
{
    char vehicle_type = 'P';
    while (running)
    {
        carOwner(&vehicle_type);
        usleep((rand() % 1000 + 100) * 1000); // Sleep for 100ms to 1s
    }
    return NULL;
}

// This function simulates a vehicle generator that generates vehicles at random intervals.
void *automobile_generator(void *arg)
{
    char vehicle_type = 'A';
    while (running)
    {
        carOwner(&vehicle_type);
        usleep((rand() % 1000 + 100) * 1000); // Sleep for 100ms to 1s
    }
    return NULL;
}

// Main function. Initializes semaphores, creates threads, and joins threads.
// Signal handler is also initialized to handle SIGINT (Ctrl+C) to gracefully exit the program.
int main()
{
    srand(time(NULL));

    struct sigaction sa;
    sa.sa_handler = intHandler;
    sigemptyset(&sa.sa_mask);

    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    // Initialize semaphores
    sem_init(&newAutomobile, 0, 0);
    sem_init(&inChargeforAutomobile, 0, 1);
    sem_init(&newPickup, 0, 0);
    sem_init(&inChargeforPickup, 0, 1);

    // Create attendant threads
    char auto_vehicle_type = 'A';
    char pickup_vehicle_type = 'P';
    pthread_create(&auto_attendant_thread, NULL, carAttendant, NULL);
    pthread_create(&pickup_attendant_thread, NULL, carAttendant, NULL);

    // Create vehicle generator threads
    pthread_create(&pickup_gen_thread, NULL, pickup_generator, NULL);
    pthread_create(&automobile_gen_thread, NULL, automobile_generator, NULL);

    // Join threads
    pthread_join(pickup_gen_thread, NULL);
    pthread_join(automobile_gen_thread, NULL);
    pthread_join(auto_attendant_thread, NULL);
    pthread_join(pickup_attendant_thread, NULL);

    // Destroy semaphores
    sem_destroy(&newAutomobile);
    sem_destroy(&inChargeforAutomobile);
    sem_destroy(&newPickup);
    sem_destroy(&inChargeforPickup);

    return 0;
}

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<fcntl.h>
#include<unistd.h>
#include<crypt.h>
#include<semaphore.h>
#include<netinet/ip.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<sys/sem.h>
#include<sys/wait.h>
#include<errno.h>
#include<signal.h>

// Compile
// gcc server.c -o server -L/lib/x86_64-linux-gnu/libcrypt.so -lcrypt -pthread

#define EMPPATH "Data/employees.txt"
#define CUSPATH "Data/customers.txt"
#define LOANPATH "Data/loanDetails.txt"
#define COUNTERPATH "Data/loanCounter.txt"
#define HISTORYPATH "Data/trans_hist.txt"
#define FEEDPATH "Data/feedback.txt"
#define HASHKEY "$6$saltsalt$"

#define MAINMENU "\n===== Login As =====\n1. Customer\n2. Employee\n3. Manager\n4. Admin\n5. Exit\nEnter your choice: "
#define ADMINMENU "\n===== Admin =====\n1. Add New Bank Employee\n2. Modify Customer/Employee Details\n3. Manage User Roles\n4. Logout\n5. Exit\nEnter your choice: "
#define CUSMENU "\n===== Customer =====\n1.  Deposit\n2.  Withdraw\n3.  View Balance\n4.  Apply for a loan\n5.  Money Transfer\n6.  Change Password\n7.  View Transaction\n8.  Add Feedback\n9.  Logout\n10. Exit\nEnter your choice: "
#define EMPMENU "\n===== Employee =====\n1. Add New Customer\n2. Modify Customer Details\n3. Approve/Reject Loans\n4. View Assigned Loan Applications\n5. View Customer Transactions\n6. Change Password\n7. Logout\n8. Exit\nEnter your choice: "
#define MNGMENU "\n===== Manager =====\n1. Activate/Deactivate Customer Accounts\n2. Assign Loan Application Processes to Employees\n3. Review Customer Feedback\n4. Change Password\n5. Logout\n6. Exit\nEnter your choice: "

void employeeMenu(int connectionFD);
void managerMenu(int connectionFD);
void adminMenu(int connectionFD);
void connectionHandler(int connectionFileDescriptor);
void exitClient(int connectionFD, int id);
void cleanupSemaphore(int signum);
void setupSignalHandlers();
sem_t *initializeSemaphore(int accountNumber);

sem_t *sema;
char semName[50];

// Shared IO buffers for request/response handling
char readBuffer[4096], writeBuffer[4096];

#include "AllStructure/allStruct.h"
#include "Modules/Customer.h"
#include "Modules/Admin.h"
#include "Modules/Employee.h"
#include "Modules/Manager.h"

#include <dirent.h>

void cleanupAllSemaphores() {
    DIR *d = opendir("/dev/shm");
    if (!d) return;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        // Remove all semaphore files created by this project
        if (strncmp(entry->d_name, "sem.sem_", 8) == 0) {
            char path[256];
            snprintf(path, sizeof(path), "/dev/shm/%.240s", entry->d_name);
            unlink(path);
        }
    }
    closedir(d);
}


int main()
{
    cleanupAllSemaphores();
    int socketFileDescriptor, connectionFileDescriptor;
    int bindStatus;
    int listenStatus;
    int clientSize;

    struct sockaddr_in address, client;
    
    // Creating Socket
    socketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if(socketFileDescriptor == -1)
    {
        perror("Error");
        exit(-1);
    }
    printf("Server Created\n");

    int opt = 1;
    if (setsockopt(socketFileDescriptor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(socketFileDescriptor);
        exit(-1);
    }

    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);

    // Binding Socket
    bindStatus = bind(socketFileDescriptor, (struct sockaddr *)&address, sizeof(address));
    if (bindStatus == -1)
    {
        perror("Error");
        exit(-1);
    }
    printf("Binding to socket successful!\n");

    // Listening for connection
    listenStatus = listen(socketFileDescriptor, 2);
    if (listenStatus == -1)
    {
        perror("Error");
        exit(-1);
    }
    printf("Listening for connections!\n");

    while(1)
    {
        clientSize = sizeof(client);
        connectionFileDescriptor = accept(socketFileDescriptor, (struct sockaddr *) &client, &clientSize);
    
        if (connectionFileDescriptor == -1)
            perror("Error");
        else
        {
            if(fork() == 0)//create child process for each new user
            {
                // Handling client connection
                connectionHandler(connectionFileDescriptor);
            }
        }
    }
    close(socketFileDescriptor);
    
    return 0;
}

// Handling client connection
void connectionHandler(int connectionFileDescriptor)
{
    char readBuffer[4096], writeBuffer[4096];
    int readBytes, writeBytes, choice;

    while(1)
    {
        writeBytes = write(connectionFileDescriptor, MAINMENU, sizeof(MAINMENU));
        if(writeBytes == -1)
        {
            printf("Unable to send data\n");
        }
        else
        {   
            bzero(readBuffer, sizeof(readBuffer));
            readBytes = read(connectionFileDescriptor, readBuffer, sizeof(readBuffer));
            if(readBytes == -1)
            {
                printf("Unable to read data from client\n");
            }
            else if(readBytes == 0)
            {
                printf("No data was sent to the server\n");
            }
            else
            {
                choice = atoi(readBuffer);
                printf("Client entered: %d\n", choice);
                switch (choice) 
                {
                    case 1:
                        // Customer
                        customerMenu(connectionFileDescriptor);
                        break;
                    
                    case 2:
                        // Employee
                        employeeMenu(connectionFileDescriptor);
                        break;
                            
                    case 3:
                        // Manager
                        managerMenu(connectionFileDescriptor);
                        break;

                    case 4:
                        // Admin
                        adminMenu(connectionFileDescriptor);
                        break;

                    case 5:
                        exitClient(connectionFileDescriptor, 0);
                        return;

                    default:
                        printf("Invalid choice! Please try again.\n");
                        break;
                }
            }
        }
    }
}

void exitClient(int connectionFileDescriptor, int id)
{
    snprintf(semName, 50, "/sem_%d", id);

    sem_t *sema = sem_open(semName, 0);
    if (sema != SEM_FAILED) {
        sem_post(sema);
        sem_close(sema); 
        sem_unlink(semName);    
    }

    bzero(writeBuffer, sizeof(writeBuffer));
    strcpy(writeBuffer, "Client logging out...\n");
    write(connectionFileDescriptor, writeBuffer, sizeof(writeBuffer));
    read(connectionFileDescriptor, readBuffer, sizeof(readBuffer));
}

// =================== Session Handling =================
void cleanupSemaphore(int signum) {
    if (sema != NULL) {
        sem_post(sema);
        sem_close(sema);
        sem_unlink(semName); 
    }
    printf("Program interrupted. Semaphore for customer cleaned up.\n");
    _exit(signum);
}

sem_t *initializeSemaphore(int id) {
    snprintf(semName, 50, "/sem_%d", id);
    return sem_open(semName, O_CREAT, 0644, 1);  // Initialize to 1
}

void setupSignalHandlers() {
    signal(SIGINT, cleanupSemaphore); 
    signal(SIGTERM, cleanupSemaphore); 
    signal(SIGSEGV, cleanupSemaphore);
    signal(SIGHUP, cleanupSemaphore);  
    signal(SIGQUIT, cleanupSemaphore);
}

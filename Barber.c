#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "ezipc.h"

//Since in our sample output, we want to see which customer the barber is serving, how
//many haircuts he has done so far and how many seats we have so far in our waiting room

//struct for allocate in shared memory
struct CShare{
    //number of haircuts today
     int HairCut_counter;
    //current number of free seats in the waiting room
     int Seat_counter;
};
struct CShare * volatile sh;

// Since the requirement states that: Your program consists of a barber process (the main program)
// and a process for each customer. In the program you must display actions taken by the customer and the barber.
// We can create the work function for the cashier and barber.

// I am going to declare the work function beforehand to avoid syntax error and help with logic

// global variable field

// global semaphore for cashier. We use it in the "pay" function
int Cashier; // think using a binary semaphore for the cashier simply is the customer paying or not


// global semaphore for  waiting room seats. We use it in the "haircut" function
int Seats; // think using a counting semaphore for the seats since we are seeing if there is seat available.

// SIG setter
int flagEnd =0;

/*
	Work Function Field 
*/

// Barber work function
void barber(int customerId)
{
    int sleepTime;
    printf("**** HAIR CUT! Customer %d is getting a haircut.\n",customerId);
    //Increment waiting room seats after the customer sits on the barber chair
    V(Seats);
    //Increase the number of seat, so we can print it out
    ++sh-> Seat_counter;
    //if we don't use sleep in our haircut it will be very fast and we will not be able to see the process interaction
    sleepTime = (rand()%4) + 1; // let it sleep for 1 to 4 sec
    sleep(sleepTime);
    //when the barber finished cutting the hair, increment the number of haircut
    ++sh-> HairCut_counter;
    printf("### Finished giving haircut to customer %d. That is haircut %d today.\n",customerId,sh->HairCut_counter);
}

// Cashier work function
void pay(int payId)
{
    //create new process
    int pay_pid=fork();
    if (pay_pid==0){
        //this is new process
        printf("Customer %d gets in line to pay\n",payId);
        //check on free cashier
        P(Cashier);
        printf("$$ Customer %d is now paying cashier\n", payId);
        sleep(1); //Sleep for 1 seconds
        //release cashier
        V(Cashier);
        printf("$$$$$$ Customer %d done paying and leaves, bye.\n",payId);
        exit(1);
    }
}
//dealing with the signal
void sig_handler (int signo)
{
    printf("\nBarber received USR1 smoke signal\n");
    flagEnd = 1;
}

/*
 BIG PICTURE:
 When we fork()
 We create 2 process and we can simply use one process and we can do all the job, and this process will be our barber process
 we can create customer process< max_number_customer in it and we have our customer ID, we can use the customer ID we got to work with the work function haircut(), pay ().
 */

int main() {
    // prompt the usr to enter input requested by the document
    int maximum_number_customers;
    int number_of_chairs;
    printf("Enter number of chairs and maximum numer of customers: ");
    scanf("%d %d", &number_of_chairs, &maximum_number_customers);
    printf("Number of chairs:%d\n", number_of_chairs);
    printf("Maximum number of customers:%d\n", maximum_number_customers);

    //setting up the semaphore as we planned
    SETUP();

    //global variable
    //counting semaphore
    Seats = SEMAPHORE(SEM_CNT, number_of_chairs);
    //binary semaphore
    Cashier = SEMAPHORE(SEM_BIN, 1);

    // We use struct as a container for all shared variables.
    //This is convenient because we can allocate memory for all our shared variables using a single operation.
    sh = (struct CShare *) SHARED_MEMORY(sizeof(struct CShare));

    sh->HairCut_counter = 0;
    sh->Seat_counter = number_of_chairs;

    //it is easier to use the pid
    int barberPID= getpid();
    signal(SIGUSR1,&sig_handler);
    printf("Barber process pid is: %d\n", barberPID);
    printf("To kill barber process when max number of customers generated and served,type the following command into a separate shell:\n kill -USR1 %d\n", barberPID);

    for (int customerCounter = 1; customerCounter <= maximum_number_customers; ++customerCounter) {

            //we should choose child process
        //our logic: the loop: in the parent process creates customerCounter new processes
            if (fork() == 0) {
            //sleep a bit before we send our customer id 
                sleep(customerCounter);
                if (sh->Seat_counter == 0) {
                // if there is no more seat in the waiting room
                    printf("OH NO! Customer %d leaves, no chairs available.\n", customerCounter);
                    
                } else {
                //if there are seats available in the waiting room
                    printf("\nCustomer %d arrived.There are %d seats available.\n", customerCounter,
                           sh->Seat_counter - 1);
                    //customer sit on the waiting room seat, the number of seats available in waiting room -1
                    P(Seats);
                    //here send the customer id to the parent process, the parent process will work on the customers
                    char buf[20];
                    //here we have the string in the buffer
                    //SEND function works with strings, not with the integer
                    sprintf(buf, "%d", customerCounter);
                    SEND(1, buf);
                    --sh->Seat_counter;

                }
               if(customerCounter==maximum_number_customers) printf("! ! ! Reached maximum number of customers: %d\n", maximum_number_customers);
                // we need to kill this process since it did its job
                exit(1);

            }
           
        } 

    while(!flagEnd)
        {
        //use the work function and work on the customer
            if(sh->Seat_counter!= number_of_chairs){
                //meaning someone is waiting
                printf("Barber awakened, or there was a customer waiting there. There are/is %d seats available in the waiting room.\n",
                       sh->Seat_counter + 1);
                //get the id of current customer
                char buf[20];
                RECEIVE(1, buf);
                int customerId = atoi(buf);

                barber(customerId);
                pay(customerId);
            }else{

                sleep(1);
            }
        }

        printf("Total number of haircuts:%d\n",sh->HairCut_counter);
        printf("Close shop and go home,bye bye!\n");
    

}

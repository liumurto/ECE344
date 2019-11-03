/* 
 * stoplight.c
 *
 * 31-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: You can use any synchronization primitives available to solve
 * the stoplight problem in this file.
 */


/*
 * 
 * Includes
 *
 */

#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>


/*
 *
 * Constants
 *
 */

struct lock *regions[4];        //create a set of locks to manage the regions being used
struct lock *front[4];          //create a set of locks for the first position of each lane in the traffic intersection
struct semaphore *intersection; //create a semaphore to manage the number of cars in the intersection at a time

/*
 * Number of cars created.
 */

#define NCARS 20


/*
 *
 * Function Definitions
 *
 */

static const char *directions[] = { "N", "E", "S", "W" };

static const char *msgs[] = {
        "approaching:",
        "region1:    ",
        "region2:    ",
        "region3:    ",
        "leaving:    "
};

/* use these constants for the first parameter of message */
enum { APPROACHING, REGION1, REGION2, REGION3, LEAVING };

static void
message(int msg_nr, int carnumber, int cardirection, int destdirection)
{
        kprintf("%s car = %2d, direction = %s, destination = %s\n",
                msgs[msg_nr], carnumber,
                directions[cardirection], directions[destdirection]);
}
 
/*
 * gostraight()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement passing straight through the
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
gostraight(unsigned long cardirection, 
           unsigned long cardestination,
           unsigned long carnumber)
{
        /*
         * Avoid unused variable warnings.
         */
        /*
         * locks the region1 this car is traveling through before it enters the corresponding region
         */
        if(cardirection == 0){
            lock_acquire(regions[3]);
        }
        else if(cardirection == 1){
            lock_acquire(regions[0]);
        }
        else if(cardirection == 2){
            lock_acquire(regions[1]);
        }
        else if(cardirection == 3){
            lock_acquire(regions[2]);
        }       

        message(REGION1, carnumber, cardirection, cardestination);
        /*
         *acquires lock for region2 before releasing region1 lock to ensure no crash
         */
        lock_acquire(regions[cardestination]);

        message(REGION2, carnumber, cardirection, cardestination);

        if(cardirection == 0){
            lock_release(regions[3]);
        }
        else if(cardirection == 1){
            lock_release(regions[0]);
        }
        else if(cardirection == 2){
            lock_release(regions[1]);
        }
        else if(cardirection == 3){
            lock_release(regions[2]);
        }       
        (void) cardirection;
        (void) carnumber;

}


/*
 * turnleft()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      This function should implement making a left turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
turnleft(unsigned long cardirection, 
         unsigned long cardestination,
         unsigned long carnumber)
{
        /*
         * Avoid unused variable warnings.
         */
        /*
         * similarly from going straight, acquire each lock before entering and stagger releasing the lock until after it enters the next region
         */
        if(cardirection == 0){
            lock_acquire(regions[3]);
        }
        else if(cardirection == 1){
            lock_acquire(regions[0]);
        }
        else if(cardirection == 2){
            lock_acquire(regions[1]);
        }
        else if(cardirection == 3){
            lock_acquire(regions[2]);
        }       

        message(REGION1, carnumber, cardirection, cardestination); 

        if(cardirection == 0){
            lock_acquire(regions[2]);
        }
        else if(cardirection == 1){
            lock_acquire(regions[3]);
        }
        else if(cardirection == 2){
            lock_acquire(regions[0]);
        }
        else if(cardirection == 3){
            lock_acquire(regions[1]);
        }    

        message(REGION2, carnumber, cardirection, cardestination);

        if(cardirection == 0){
            lock_release(regions[3]);
        }
        else if(cardirection == 1){
            lock_release(regions[0]);
        }
        else if(cardirection == 2){
            lock_release(regions[1]);
        }
        else if(cardirection == 3){
            lock_release(regions[2]);
        }    

        lock_acquire(regions[cardestination]);

        message(REGION3, carnumber, cardirection, cardestination);

        if(cardirection == 0){
            lock_release(regions[2]);
        }
        else if(cardirection == 1){
            lock_release(regions[3]);
        }
        else if(cardirection == 2){
            lock_release(regions[0]);
        }
        else if(cardirection == 3){
            lock_release(regions[1]);
        }   

        (void) cardirection;
        (void) carnumber;

}


/*
 * turnright()
 *
 * Arguments:
 *      unsigned long cardirection: the direction from which the car
 *              approaches the intersection.
 *      unsigned long carnumber: the car id number for printing purposes.
 *
 * Returns:
 *      nothing. 
 *
 * Notes:
 *      This function should implement making a right turn through the 
 *      intersection from any direction.
 *      Write and comment this function.
 */

static
void
turnright(unsigned long cardirection, 
          unsigned long cardestination,
          unsigned long carnumber)
{
        /*
         * Avoid unused variable warnings.
         */
        /*
         *only one region to pass through for turning right, so acquire it and still stagger releasing until it leaves the intersection
        */
        lock_acquire(regions[cardestination]);

        message(REGION1, carnumber, cardirection, cardestination);

        (void) cardirection;
        (void) carnumber;

}


/*
 * approachintersection()
 *
 * Arguments: 
 *      void * unusedpointer: currently unused.
 *      unsigned long carnumber: holds car id number.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Change this function as necessary to implement your solution. These
 *      threads are created by createcars().  Each one must choose a direction
 *      randomly, approach the intersection, choose a turn randomly, and then
 *      complete that turn.  The code to choose a direction randomly is
 *      provided, the rest is left to you to implement.  Making a turn
 *      or going straight should be done by calling one of the functions
 *      above.
 */
 
static
void
approachintersection(void * unusedpointer,
                     unsigned long carnumber)
{
        int cardirection;
        int cardestination;

        /*
         * Avoid unused variable and function warnings.
         */

        (void) unusedpointer;
        (void) carnumber;
	    (void) gostraight;
	    (void) turnleft;
	    (void) turnright;

        /*
         * cardirection is set randomly.
         */
        cardirection = random() % 4;


        P(intersection);                                                                            //keep track of each car entering the intersection
        lock_acquire(front[cardirection]);                                                          //ensure that there is only one car in front position to avoid jumping in queue                           

        cardestination = random() % 4;
        while(cardestination == cardirection){                                                      //make sure the cars are not trying to go the way they came
            cardestination = random() % 4;
        }

        message(APPROACHING, carnumber, cardirection, cardestination);                              //message to denote a car approaching the intersection
        if((cardestination == (cardirection + 2))||(cardestination == (cardirection - 2))){
            kprintf("Car number %d is going straight\n", carnumber);
            gostraight(cardirection, cardestination, carnumber);
        }
        else if((cardestination == (cardirection - 1))||(cardestination == (cardirection + 3))){
            kprintf("Car number %d is going right\n", carnumber);            
            turnright(cardirection, cardestination, carnumber);
        }
        else if((cardestination == (cardirection + 1))||(cardestination == (cardirection - 3))){    
            kprintf("Car number %d is going left\n", carnumber);
            turnleft(cardirection, cardestination, carnumber);
        }
        message(LEAVING, carnumber, cardirection, cardestination);
        /*
         *ensure no crashes happen at the very end by not releasing the lock until the car has entirely left the intersection
         */
        lock_release(regions[cardestination]);
        lock_release(front[cardirection]);
        V(intersection);

        thread_yield();
}


/*
 * createcars()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up the approachintersection() threads.  You are
 *      free to modiy this code as necessary for your solution.
 */

int
createcars(int nargs,
           char ** args)
{
        int index, error;

        /*
         * Avoid unused variable warnings.
         */

        (void) nargs;
        (void) args;

        regions[0] = lock_create("NE");
        regions[1] = lock_create("SE");
        regions[2] = lock_create("SW");
        regions[3] = lock_create("NW");
        front[0] = lock_create("NE");
        front[1] = lock_create("NW");
        front[2] = lock_create("SE");
        front[3] = lock_create("SW");
        intersection = sem_create("Intersection", 3);

        /*
         * Start NCARS approachintersection() threads.
         */

        for (index = 0; index < NCARS; index++) {

                error = thread_fork("approachintersection thread",
                                    NULL,
                                    index,
                                    approachintersection,
                                    NULL
                                    );

                /*
                 * panic() on error.
                 */

                if (error) {
                        
                        panic("approachintersection: thread_fork failed: %s\n",
                              strerror(error)
                              );
                }
        }

        return 0;
}

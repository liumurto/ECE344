/*
 * catlock.c
 *
 * 30-1-2003 : GWA : Stub functions created for CS161 Asst1.
 *
 * NB: Please use LOCKS/CV'S to solve the cat syncronization problem in 
 * this file.
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
#include <synch.h>


/*
 * 
 * Constants
 *
 */

/*
 * Number of food bowls.
 */

#define NFOODBOWLS 2

/*
 * Number of cats.
 */

#define NCATS 6

/*
 * Number of mice.
 */

#define NMICE 2

static int num_cats_eating;

struct lock *bowl_lock[2];  //2 locks for each bowl
struct lock *lock_for_cat_and_mouse; //general lock for animal
const char *cat = "cat";
const char *mouse = "mouse";
/*
 * 
 * Function Definitions
 * 
 */

/* who should be "cat" or "mouse" */
static void
lock_eat(const char *who, int num, int bowl, int iteration)
{
        kprintf("%s: %d starts eating: bowl %d, iteration %d\n", who, num, 
                bowl, iteration);
        clocksleep(1);
        kprintf("%s: %d ends eating: bowl %d, iteration %d\n", who, num, 
                bowl, iteration);
}


/*
 * catlock()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long catnumber: holds the cat identifier from 0 to NCATS -
 *      1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using locks/cv's.
 *
 */

static
void
catlock(void * unusedpointer, unsigned long catnumber){

    (void) unusedpointer;
    int iteration = 0;
    int bowl_num = (int)(random()%2)+1;  //random value for bowl (1 or 2)
    for(iteration = 0; iteration <= 3;){
        
        lock_acquire(lock_for_cat_and_mouse);

        if(num_cats_eating > 1)
            lock_release(lock_for_cat_and_mouse); //cat can't eat if two or more cats are eating now
        else
        {
            num_cats_eating++;
            lock_release(lock_for_cat_and_mouse);

            lock_acquire(bowl_lock[bowl_num]);
            lock_eat(cat, catnumber, bowl_num, iteration);
            lock_release(bowl_lock[bowl_num]);

            lock_acquire(lock_for_cat_and_mouse);     
            num_cats_eating--;
            lock_release(lock_for_cat_and_mouse); //cat finished eating
            iteration++; //proceed to next iteration for cat
        }
        thread_yield();
    } 
}
    

/*
 * mouselock()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long mousenumber: holds the mouse identifier from 0 to 
 *              NMICE - 1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Write and comment this function using locks/cv's.
 *
 */

static
void
mouselock(void * unusedpointer, unsigned long mousenumber){

    (void) unusedpointer; 
    int iteration = 0;
    int bowl_num = (int)(random()%2)+1;
    for(iteration = 0; iteration <= 3;){
    
        lock_acquire(lock_for_cat_and_mouse);

        if(num_cats_eating == 1 || num_cats_eating == 2)  //mouse can't eat while any cat is eating
            lock_release(lock_for_cat_and_mouse);
        else
        {
            if(num_cats_eating != 0)
              num_cats_eating++;   
            else
                num_cats_eating = 3; //so cats will have to wait while mouse will eat
                                      // because cat can't eat while number of cats eating at a moment more than 2
            lock_release(lock_for_cat_and_mouse);

            lock_acquire(bowl_lock[bowl_num]);
            lock_eat(mouse, mousenumber, bowl_num, iteration);
            lock_release(bowl_lock[bowl_num]);
     
            lock_acquire(lock_for_cat_and_mouse);
            if(num_cats_eating == 3)
                num_cats_eating = 0;   //after mouse finished to eat cats can proceed
            else
                num_cats_eating--; 
            lock_release(lock_for_cat_and_mouse); 
            iteration++;
        }
        thread_yield();
    }
}


/*
 * catmouselock()
 *
 * Arguments:
 *      int nargs: unused.
 *      char ** args: unused.
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up catlock() and mouselock() threads.  Change
 *      this code as necessary for your solution.
 */

int
catmouselock(int nargs, char ** args){

        int index, error;

        (void) nargs;
        (void) args;

        num_cats_eating = 0;
        bowl_lock[0] = lock_create("bowl_lock_1");
        bowl_lock[1] = lock_create("bowl_lock_2");
        lock_for_cat_and_mouse = lock_create("some_lock");
        
   
        /*
         * Start NCATS catlock() threads.
         */

        for (index = 0; index < NCATS; index++) {
           
                error = thread_fork("catlock thread", 
                                    NULL, 
                                    index, 
                                    catlock, 
                                    NULL
                                    );
                
                /*
                 * panic() on error.
                 */

                if (error) {
                 
                        panic("catlock: thread_fork failed: %s\n", 
                              strerror(error)
                              );
                }
        }

        /*
         * Start NMICE mouselock() threads.
         */

        for (index = 0; index < NMICE; index++) {
   
                error = thread_fork("mouselock thread", 
                                    NULL, 
                                    index, 
                                    mouselock, 
                                    NULL
                                    );
      
                /*
                 * panic() on error.
                 */

                if (error) {
         
                        panic("mouselock: thread_fork failed: %s\n", 
                              strerror(error)
                              );
                }
        }

        return 0;
}

/*
 * End of catlock.c
 */
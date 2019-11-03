/*
 * Synchronization primitives.
 * See synch.h for specifications of the functions.
 */

#include <types.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <machine/spl.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *namearg, int initial_count)
{
	struct semaphore *sem;

	assert(initial_count >= 0);

	sem = kmalloc(sizeof(struct semaphore));
	if (sem == NULL) {
		return NULL;
	}

	sem->name = kstrdup(namearg);
	if (sem->name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->count = initial_count;
	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	spl = splhigh();
	assert(thread_hassleepers(sem)==0);
	splx(spl);

	/*
	 * Note: while someone could theoretically start sleeping on
	 * the semaphore after the above test but before we free it,
	 * if they're going to do that, they can just as easily wait
	 * a bit and start sleeping on the semaphore after it's been
	 * freed. Consequently, there's not a whole lot of point in 
	 * including the kfrees in the splhigh block, so we don't.
	 */

	kfree(sem->name);
	kfree(sem);
}

void 
P(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	assert(in_interrupt==0);

	spl = splhigh();
	while (sem->count==0) {
		thread_sleep(sem);
	}
	assert(sem->count>0);
	sem->count--;
	splx(spl);
}

void
V(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);
	spl = splhigh();
	sem->count++;
	assert(sem->count>0);
	thread_wakeup(sem);
	splx(spl);
}

////////////////////////////////////////////////////////////
//
// Lock.
//For each lock function, make sure the lock both exists and is held by the thread attempting to operate the lock if relevant
struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(struct lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->name = kstrdup(name);
	if (lock->name == NULL) {
		kfree(lock);
		return NULL;
	}
	
	lock->threadHolder = curthread;
	lock->isHeld = 0;                     

	return lock;
}
void
lock_destroy(struct lock *lock)
{
	int spl;

	assert(lock != NULL);     
	assert(lock->isHeld == 0);    //check if the lock is held

	spl = splhigh();	         
	assert(thread_hassleepers(lock)==0);  //make sure no threads are sleeping on specified address
	splx(spl);                //set priority level to high

	kfree(lock->name);
	kfree(lock->threadHolder);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	int spl;
	assert(lock != NULL);        
	spl = splhigh();

	while(lock->isHeld == 1){
		thread_sleep(lock);          // only one thread can hold the lock at the same time     
	}

	lock->threadHolder = curthread;
	lock->isHeld= 1;

	splx(spl);
}

void
lock_release(struct lock *lock)
{
	int spl;
	assert(lock != NULL);
	spl = splhigh();

	if(lock_do_i_hold(lock)){          //release lock only if it is held by a thread
		lock->threadHolder = NULL;        
		lock->isHeld = 0;
		thread_wakeup(lock);
	}

	splx(spl);
}

int
lock_do_i_hold(struct lock *lock)
{
	
	assert(lock != NULL);
	if(lock->isHeld == 0)                //return 1 if lock is held by thread, otherwise return 0
		return 0;
	else if(lock->threadHolder == curthread){
		return 1;
	}

	return 0;
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(struct cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->name = kstrdup(name);
	if (cv->name==NULL) {
		kfree(cv);
		return NULL;
	}
	
	cv->lock_hold = lock_create(name);
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	int spl;
	assert(cv != NULL);

	spl = splhigh();
	assert(thread_hassleepers(cv)==0);
	splx(spl);
	
	kfree(cv->name);
	kfree(cv->lock_hold);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	
	int spl;
	assert(lock != NULL);  
	assert(cv != NULL); 

	spl = splhigh();
 	lock_acquire(cv->lock_hold);
	lock_release(lock);
	lock_release(cv->lock_hold);

	thread_sleep(cv);

	lock_acquire(lock);
	splx(spl);

}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	int spl;
	assert(lock != NULL);  
	assert(cv != NULL); 
	spl = splhigh();
	
	
	thread_wakeup_1(cv);
	
	splx(spl);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	int spl;
	assert(lock != NULL);  
	assert(cv != NULL); 
	spl = splhigh();
 
	thread_wakeup(cv);
	splx(spl);
}

/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
        struct semaphore *sem;

        sem = kmalloc(sizeof(*sem));
        if (sem == NULL) {
                return NULL;
        }

        sem->sem_name = kstrdup(name);
        if (sem->sem_name == NULL) {
                kfree(sem);
                return NULL;
        }

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
        sem->sem_count = initial_count;

        return sem;
}

void
sem_destroy(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
        kfree(sem->sem_name);
        kfree(sem);
}

void
P(struct semaphore *sem)
{
        KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
        KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
        while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
        }
        KASSERT(sem->sem_count > 0);
        sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

        sem->sem_count++;
        KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
        struct lock *lock;

        lock = kmalloc(sizeof(*lock));
        if (lock == NULL) {
                return NULL;
        }

        lock->lk_name = kstrdup(name);
        if (lock->lk_name == NULL) {
                kfree(lock);
                return NULL;
        }

        // add stuff here as needed
	//When lock is created, no thread should be holding it
	lock->lk_held = false;
	lock->lk_holder = NULL;
	
	//initialize wait channel same as other structs
	lock->lk_wchan = wchan_create(lock->lk_name);
	if (lock->lk_wchan == NULL) {
                kfree(lock->lk_name);
		kfree(lock);
                return NULL;
        }	

	//initialize spinlock same as other structs
	spinlock_init(&lock->lk_spinlock);	

        return lock;
}

void
lock_destroy(struct lock *lock)
{
        KASSERT(lock != NULL);

        // add stuff here as needed
	//No thread should hold this before it is freed
	lock->lk_held = false;
	lock->lk_holder = NULL;

	//Following others functions, cleanup spinlock and destroy waitchannel
	wchan_destroy(lock->lk_wchan);
	spinlock_cleanup(&lock->lk_spinlock);

        kfree(lock->lk_name);
        kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
        // Write this
	//assert that our lock exists (duh)
	KASSERT(lock != NULL);
	//Assert there is not an interrupt in our current thread
	KASSERT(curthread->t_in_interrupt == false);	


	//since we are using volatile memory (lock->lk_held) we must acquire a spinlock	
	spinlock_acquire(&lock->lk_spinlock);
	
	while(lock->lk_holder != NULL) {
		//keeps the lock sleeping until it is no longer held
		wchan_sleep(lock->lk_wchan, &lock->lk_spinlock);			
	}
	
	lock->lk_held = true;
	lock->lk_holder = curthread;

	spinlock_release(&lock->lk_spinlock);
}

void
lock_release(struct lock *lock)
{
	KASSERT(lock != NULL);
	KASSERT(curthread != NULL);
	//only if the current thread is the holder of this lock may it release it
	if(lock_do_i_hold(lock)) {	
		spinlock_acquire(&lock->lk_spinlock);
		//free lock
		lock->lk_held = false;
		lock->lk_holder = NULL;
		wchan_wakeone(lock->lk_wchan, &lock->lk_spinlock);
		spinlock_release(&lock->lk_spinlock);
	}	
}

bool
lock_do_i_hold(struct lock *lock)
{
	//acquire spinlock to keep this operation atomic as required
	spinlock_acquire(&lock->lk_spinlock);
	
	if(lock->lk_holder == curthread) {
		spinlock_release(&lock->lk_spinlock);
		return true;
	}
	
	spinlock_release(&lock->lk_spinlock);
	return false;
}

/*//////////////////////////////////////////////////////////
//
// CV

 *    cv_wait      - Release the supplied lock, go to sleep, and, after
 *                   waking up again, re-acquire the lock.
 *    cv_signal    - Wake up one thread that's sleeping on this CV.
 *    cv_broadcast - Wake up all threads sleeping on this CV.
*/

struct cv *
cv_create(const char *name)
{
        struct cv *cv;

        cv = kmalloc(sizeof(*cv));
        if (cv == NULL) {
                return NULL;
        }

        cv->cv_name = kstrdup(name);
        if (cv->cv_name==NULL) {
                kfree(cv);
                return NULL;
        }

        // add stuff here as needed
	//initialize waitchannel
	cv->cv_wchan = wchan_create(cv->cv_name);
	if(cv->cv_wchan==NULL) {
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}
	
	spinlock_init(&cv->cv_spinlock);

        return cv;
}

void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);

        // add stuff here as needed
	wchan_destroy(cv->cv_wchan);
	spinlock_cleanup(&cv->cv_spinlock);
		
        kfree(cv->cv_name);
        kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	KASSERT(lock_do_i_hold(lock));
	
	spinlock_acquire(&cv->cv_spinlock);

	//release supplied lock
	lock_release(lock);
	//go to sleep
	wchan_sleep(cv->cv_wchan, &cv->cv_spinlock);
	//wakeup / reacquire lock	
	spinlock_release(&cv->cv_spinlock);

	//released spinlock before calling lock_acquire because lock_acquire is already atomic/has its own spinlock
	
	lock_acquire(lock);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	
	KASSERT(lock_do_i_hold(lock));

	spinlock_acquire(&cv->cv_spinlock);
	//wake up one thread sleeping
	wchan_wakeone(cv->cv_wchan, &cv->cv_spinlock);
	
	spinlock_release(&cv->cv_spinlock);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	KASSERT(lock_do_i_hold(lock));

	spinlock_acquire(&cv->cv_spinlock);
	//wake up all sleeping threads
	wchan_wakeall(cv->cv_wchan, &cv->cv_spinlock);
	
	spinlock_release(&cv->cv_spinlock);
}

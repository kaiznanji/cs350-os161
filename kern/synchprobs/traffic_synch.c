#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
static struct lock *intersection_lock;
static struct cv *N;
static struct cv *S;
static struct cv *W;
static struct cv *E;

static int inside[4];
static int wait[4];

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */

  intersection_lock = lock_create("intersection_lock");
  if (intersection_lock == NULL) {
    panic("could not create intersection lock");
  }
  
  N = cv_create("N");
  if (N == NULL) panic("could not create N cv");
  
  S = cv_create("S");
  if (S == NULL) panic("could not create S cv");
  
  W = cv_create("W");
  if (W == NULL) panic("could not create W cv");
  
  E = cv_create("E");
  if (E == NULL) panic("could not create E cv");
  
  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
	KASSERT(intersection_lock != NULL);
	
  	lock_destroy(intersection_lock);
  	
	KASSERT(N != NULL);
  	cv_destroy(N);
  	
	KASSERT(S != NULL);
  	cv_destroy(S);
  	
	KASSERT(W != NULL);
  	cv_destroy(W);
  	
	KASSERT(E != NULL);
  	cv_destroy(E);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
	KASSERT(intersection_lock != NULL);
	(void)destination;
	lock_acquire(intersection_lock);

	if (origin == north) {
		++wait[0];
		while (!(inside[1] == 0 && inside[2] == 0 && inside[3] == 0)) {
			cv_wait(N, intersection_lock);
		}
		++inside[0];
		--wait[0];
	} else if (origin == south) {
		++wait[1];
		while (!(inside[0] == 0 && inside[2] == 0 && inside[3] == 0)) {
                       cv_wait(S, intersection_lock);
                }
		--wait[1];
                ++inside[1];
        } else if (origin == east) {
        	++wait[2];
		while (!(inside[0] == 0 && inside[1] == 0 && inside[3] == 0)) {
                       cv_wait(E, intersection_lock);
                }
		--wait[2];
                ++inside[2];
	} else {
		++wait[3];
                while (!(inside[0] == 0 && inside[1] == 0 && inside[2] == 0)) {
                       cv_wait(W, intersection_lock);
                }
		--wait[3];
                ++inside[3];
        }

	lock_release(intersection_lock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
	KASSERT(intersection_lock != NULL);
        (void)destination;
	lock_acquire(intersection_lock);
	
	int newDirection;
	int max = 0;
	for (int i = 0; i < 4; ++i) {
		if (wait[i] > max) {
			max = wait[i];
			newDirection = i;
		}
	}
	if (origin == north) {
                KASSERT(inside[0] > 0);
		--inside[0];
		if (inside[0] != 0) newDirection = -1;
	} else if (origin == south) {
		KASSERT(inside[1] > 0);
                --inside[1];
		if (inside[1] != 0) newDirection = -1;
        } else if (origin == east) {
                KASSERT(inside[2] > 0);
		--inside[2];
		if (inside[2] != 0) newDirection = -1;
        } else {
		KASSERT(inside[3] > 0);
                --inside[3];
		if (inside[3] != 0) newDirection = -1;
        }

	if (newDirection == 0) {
		cv_broadcast(N, intersection_lock);
	} else if (newDirection == 1) {
                cv_broadcast(S, intersection_lock);
        } else if (newDirection == 2) {
                cv_broadcast(E, intersection_lock);
        } else if (newDirection == 3) {
                cv_broadcast(W, intersection_lock);
        }


        lock_release(intersection_lock);
}

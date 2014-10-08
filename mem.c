#include "stagemedia.h"

pthread_mutex_t mem_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pre_mem_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t	current_lock = -1;

int	MemLock(void) {
	/* Get a "pre" lock -- this prevents deadlock situations */
	pthread_mutex_lock(&pre_mem_mutex);

	if (current_lock > 0 && current_lock == pthread_self()) {
		pthread_mutex_unlock(&pre_mem_mutex);
		return 0;
	}

	pthread_mutex_lock(&mem_mutex);
	current_lock = pthread_self();
	pthread_mutex_unlock(&pre_mem_mutex);

	return 0;
//	return pthread_mutex_lock(&mem_mutex);
}

int	atomic(void) {
	return MemLock();
}

int	MemUnlock(void) {
	current_lock = -1;
	pthread_mutex_unlock(&mem_mutex);
	return 0;
//	return pthread_mutex_unlock(&mem_mutex);
}

int	noatomic(void) {
	return MemUnlock();
}


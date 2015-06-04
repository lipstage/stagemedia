#include "stagemedia.h"

pthread_mutex_t mem_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pre_mem_mutex = PTHREAD_MUTEX_INITIALIZER;
static		int	is_glob_locked;
pthread_t	current_lock = -1;

int	__not_used_MemLock(void) {
	fprintf(stderr, "%s:%d (locking)\n", __FUNCTION__, __LINE__);

	/* Get a "pre" lock -- this prevents deadlock situations */
	pthread_mutex_lock(&pre_mem_mutex);

	if (is_glob_locked > 0 && current_lock == pthread_self()) {
		pthread_mutex_unlock(&pre_mem_mutex);
		return 0;
	}

	pthread_mutex_lock(&mem_mutex);
	current_lock = pthread_self();
	is_glob_locked++;
	pthread_mutex_unlock(&pre_mem_mutex);

	fprintf(stderr, "%s:%d (locked)\n", __FUNCTION__, __LINE__);

	return 0;
//	return pthread_mutex_lock(&mem_mutex);
}

#ifdef	CONSOLE_DEBUGGING
int     MemLock_Assert(char *func, int line) {
#else
int	MemLock(void) {
#endif

#ifdef	CONSOLE_DEBUGGING
	static	char	last_success[512];
	static	int		last_line;

	if (*last_success) {
		fprintf(stderr, "MemLock_Assert: Last successful lock (function = %s, line = %d)\n", last_success, last_line);
		/* debug3("MemLock_Assert: Last successful lock (function = %s, line = %d)", last_success, last_line); */
	}
	fprintf(stderr, "MemLock_Assert: Now trying to lock (function = %s, line = %d)\n", func, line);
#endif

        /* Get a "pre" lock -- this prevents deadlock situations */
        pthread_mutex_lock(&pre_mem_mutex);

        if (is_glob_locked > 0 && current_lock == pthread_self()) {
                pthread_mutex_unlock(&pre_mem_mutex);
                return 0;
        }

	pthread_mutex_lock(&mem_mutex);
        current_lock = pthread_self();
	is_glob_locked++;
        pthread_mutex_unlock(&pre_mem_mutex);

#ifdef CONSOLE_DEBUGGING
	strncpy(last_success, func, sizeof last_success - 1);
	last_line = line;
#endif
        return 0;
}

int	atomic(void) {
	return MemLock();
}

int	MemUnlock(void) {
	current_lock = -1;
	is_glob_locked = 0;
	pthread_mutex_unlock(&mem_mutex);
	return 0;
}

int	noatomic(void) {
	return MemUnlock();
}


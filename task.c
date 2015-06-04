#include "stagemedia.h"

pthread_mutex_t task_mutex = PTHREAD_MUTEX_INITIALIZER;

pThreads	AllThreadsHead = NULL;

/*
 * Create a new task, which is a sub-process more or less
 */
pThreads	new_task(void) {
	pThreads	p;
	pBytes		rbuf, wbuf;

	/* Attempt to allocate, if at all possible */
	if (!(p = calloc(1, sizeof(*p))))
		return NULL;

	if (!(rbuf = bytes_init())) {
		free (p);
		return NULL;
	}

	if (!(wbuf = bytes_init())) {
		bytes_free(rbuf);
		free (p);
		return NULL;
	}

	/* We want the task's to use a ring buffer */
	bytes_type_set(rbuf, BYTES_TYPE_RING);
	bytes_type_set(wbuf, BYTES_TYPE_RING); 

	/* The maximum size is 1 megabyte */
	/* reduced to 100k  -kf */
	/* moved to a #define in config.h -kf */
	bytes_maxsize_set(rbuf, BYTES_MAXSIZE_PER_USER);
	bytes_maxsize_set(wbuf, BYTES_MAXSIZE_PER_USER);

	/* Set next to NULL now */
	p->next = NULL;

#if defined(INIT_BURST_FLASHBACK) && defined(INIT_BURST_ON_CONNECT)
	p->flashback = p->init = 0;
#endif

	p->stats.wbuf_sent = 0;

	/* Need exclusive access */
	//MemLock();
	MEMLOCK(1);

	/* Nothing set, assign it to the linked list of head */
	if (!AllThreadsHead)
		AllThreadsHead = p;
	else {
		pThreads	scan = AllThreadsHead;

		while (scan->next)
			scan = scan->next;
		scan->next = p;
	}

	/* And done! */
	//MemUnlock();
	MEMUNLOCK(1);

	/* pBytes should be NULL */
	p->rbuf = rbuf;
	p->wbuf = wbuf;

	p->ad_inject = p->request_end = 0;
	p->sock = NULL;
	pthread_mutex_init(&p->lock, NULL);

	memset(p->identity, 0, sizeof p->identity);

	return p;
}

int	task_lock(pThreads t) {
	return 0;
	int	x;

	debug3("%s: Acquiring", __FUNCTION__);
	while (task_gislocked())
		mypause_time(10);
	x = pthread_mutex_lock(&t->lock);
	debug3("%s: Success", __FUNCTION__);
	return x;
}

int	task_lock_no_global_check(pThreads t) {
	return pthread_mutex_lock(&t->lock);
}

int	task_unlock(pThreads t) {
	int	x;

	debug3("task_unlock: Removing task lock");
	x = pthread_mutex_unlock(&t->lock);
	debug3("task_unlock: Successful");
	return x;
}

int	task_glock(void) {
	int	x;

	debug3("task_glock: Acquiring global task lock"); 
	x = pthread_mutex_lock(&task_mutex);
	debug3("task_glock: Successful");
	return x;
}

int	task_gunlock(void) {
	int	 x;

	debug3("task_gunlock: Removing global task lock");
	x = pthread_mutex_unlock(&task_mutex);
	debug3("task_gunlock: Successful");

	return x;
}

int	task_gislocked(void) {
	if (!(pthread_mutex_trylock(&task_mutex))) {
		pthread_mutex_unlock(&task_mutex);
		return 0;
	}
	return !0;
}

/*
 * Remove a task (delete the subprocess)
 */
void	task_remove(pThreads s) {
	pThreads	prev, scan;

	for (scan = AllThreadsHead, prev = NULL; scan; prev = scan, scan = scan->next) {
		if (s == scan) {
			if (prev)
				prev->next = scan->next;
			else
				AllThreadsHead = scan->next;
			pthread_mutex_destroy(&scan->lock);
			free (scan);
			return;
		}
	}
}

/*
 * When a task is done, it calls task_finish for cleanup.
 *
 *	- Acquires lock
 *	- Frees the read buffer
 *	- Closes the file descriptor
 *	- Sets itself to be "done" -- cleanup will completely remove later
 */
void	task_finish(pThreads s) {
	int	n = 0;

	debug3("task_finish: Entered function");

	/* We should make this an atomic operation */
	//MemLock();
	MEMLOCK(1);

	debug3("task_finish: Acquiring Task Lock");
	task_lock(s);

	/* free any buffers which we may have */
	bytes_free(s->rbuf);
	bytes_free(s->wbuf);

	/* Set the buffer to NULL */
	s->rbuf = NULL;
	s->wbuf = NULL;

	/* get rid of the identity */
	*s->identity = '\0';

	/* close the socket */
	sock_close(s->sock);

	/* No socket data */
	s->sock = NULL;

	// MEMUNLOCK(1);

	/* Set it as finished */
	//SetStatus_Finish(s);

	/* Unlock our lock */
	//MemUnlock();
	MEMUNLOCK(1);

	/* get rid of the task lock */
	task_unlock(s);

	/* Set it as finished */
	SetStatus_Finish(s);

	/* And exit */
	pthread_exit(&n);
}


/*
 * Performs a quick count of the number of tasks we have
 */
int	task_count(void) {
	pThreads	scan;
	register	int	count = 0;

	MEMLOCK(1);
	for (scan = AllThreadsHead; scan; scan = scan->next)
		++count;
	MEMUNLOCK(1);
	return (count);
}

int	task_string_status(pThreads p, char *buffer, int size) {
	struct {
		int	code;
		char	*name;
	} data[] = {
	{	0,	"None"		},
	{	1,	"begin"		},
	{	2,	"waitrq"	},
	{	3,	"badkey"	},
	{	4,	"prestream"	},
	{	5,	"stream"	},
	{	6,	"finish"	},
	{	7,	"dead"		},
	{	8,	"stats"		},
	{	9,	"encoder"	},
	{	255,	NULL		}
	};
	int	i;

	for (i = 0; data[i].name; i++) {
		if (data[i].code == (p->status & 0xf)) {
			strncpy(buffer, data[i].name, size - 1);
			return 0;
		}
	}
	strncpy(buffer, "unknown", size - 1);
	return 0;
}

int	task_valid(pThreads who) {
	pThreads	scan;

	for (scan = AllThreadsHead; scan; scan = scan->next)
		if (who == scan)
			return !0;
	return 0;
}

pThreads	task_by_identity(const char *identity, int do_lock) {
	pThreads	scan;

	task_glock();
	for (scan = AllThreadsHead; scan; scan = scan->next)
		if (!strcmp(scan->identity, identity)) {
			if (do_lock) 
				task_lock_no_global_check(scan);
			task_gunlock();
			return scan;
		}
	task_gunlock();
	return NULL;
}

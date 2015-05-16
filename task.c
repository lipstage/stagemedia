#include "stagemedia.h"

pThreads	AllThreadsHead = NULL;

/*
 * Create a new task, which is a sub-process more or less
 */
pThreads	new_task(void) {
	pThreads	p;
	pBytes		temp;

	/* Attempt to allocate, if at all possible */
	if (!(p = calloc(1, sizeof(*p))))
		return NULL;

	if (!(temp = bytes_init())) {
		free (p);
		return NULL;
	}

	/* We want the task's to use a ring buffer */
	bytes_type_set(temp, BYTES_TYPE_RING);

	/* The maximum size is 1 megabyte */
	/* reduced to 100k  -kf */
	/* moved to a #define in config.h -kf */
	bytes_maxsize_set(temp, BYTES_MAXSIZE_PER_USER);

	/* Set next to NULL now */
	p->next = NULL;

	/* Set the values to NULL and zero */
	p->wrb.buf = p->rdb.buf = NULL;
	p->wrb.si = p->rdb.si = 0;
#if defined(INIT_BURST_FLASHBACK) && defined(INIT_BURST_ON_CONNECT)
	p->flashback = 0;
#endif

	/* Need exclusive access */
	//MemLock();

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

	/* pBytes should be NULL */
	p->rbuf = temp;

	p->ad_inject = 0;

	return p;
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

	/* We should make this an atomic operation */
	MemLock();

	/* free any buffers which we may have */
	bytes_free(s->rbuf);

	/* Set the buffer to NULL */
	s->rbuf = NULL;

	/* close the socket */
	sock_close(s->sock);

	/* No socket data */
	s->sock = NULL;

	/* Set it as finished */
	SetStatus_Finish(s);

	/* Unlock our lock */
	MemUnlock();

	/* And exit */
	pthread_exit(&n);
}


/*
 * Performs a quick count of the number of tasks we have
 */
int	task_count(void) {
	pThreads	scan;
	register	int	count = 0;

	MemLock();
	for (scan = AllThreadsHead; scan; scan = scan->next)
		++count;
	MemUnlock();
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

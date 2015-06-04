#ifndef	__MEM_H__
#define	__MEM_H__
#include "stagemedia.h"

#define	SSTAT_NONE		0
#define	SSTAT_BEGIN		1
#define	SSTAT_WAITRQ		2
#define	SSTAT_BADKEY		3
#define	SSTAT_PRESTREAM		4
#define	SSTAT_STREAM		5
#define	SSTAT_FINISH		6
#define	SSTAT_DEAD		7
#define	SSTAT_STATS		8
#define	SSTAT_ENCODER		9	/* Implies "stream" but means the socket is NOT connected! */
#define	SSTAT_VIRTUAL		SSTAT_ENCODER

#define	SetStatus_Raw(s, num)	{ s->status = ((s->status & ~0xf) | num); }

#define	SetStatus_Begin(s)	SetStatus_Raw(s, SSTAT_BEGIN)
#define	SetStatus_WaitRQ(s)	SetStatus_Raw(s, SSTAT_WAITRQ)
#define	SetStatus_BadKey(s)	SetStatus_Raw(s, SSTAT_BADKEY)
#define	SetStatus_PreStream(s)	SetStatus_Raw(s, SSTAT_PRESTREAM)
#define	SetStatus_Stream(s)	SetStatus_Raw(s, SSTAT_STREAM)
#define	SetStatus_Finish(s)	SetStatus_Raw(s, SSTAT_FINISH)
#define	SetStatus_Dead(s)	SetStatus_Raw(s, SSTAT_DEAD)
#define SetStatus_Stats(s)	SetStatus_Raw(s, SSTAT_STATS)
#define	SetStatus_Virtual(s)	SetStatus_Raw(s, SSTAT_VIRTUAL)

#define	IsStatusWaitRQ(s)	((s->status & 0xf) == SSTAT_WAITRQ)
#define	IsStatusStream(s)	((s->status & 0xf) == SSTAT_STREAM)
#define IsStatusFinish(s)	((s->status & 0xf) == SSTAT_FINISH)
#define	IsStatusDead(s)		((s->status & 0xf) == SSTAT_DEAD)
#define	IsStatusVirtual(s)	((s->status & 0xf) == SSTAT_VIRTUAL)

#define IsAdvertising(s)	((s->ad_inject))
#define	SetAdvertising(s)	s->ad_inject=!0
#define	UnSetAdvertising(s)	s->ad_inject=0

#define	IsStreamable(x)		(IsStatusVirtual(x) || IsStatusStream(x))

typedef struct	st_Threads {
	pSocket		sock;
	pthread_t	handler;
	pBytes	rbuf;
	pBytes	wbuf;

	/*
	 * The HTTP request being made
	 * Note: not valid for encoders
	 */
	HTTPRequest	request;
	
	/*
	 * Hold statistics related to this specific stream.
	 */
	struct {
		unsigned long long int wbuf_sent;	/* bytes sent in lifetime */
	} stats;

	void	*Extra;

	/*
	 * The last time access was made to the encoder. For the typical HTTP server end,
	 * this really has no effect.
	 */
	time_t	last;

	unsigned int	status;			/* the "status" of the thread (encoder/virtual or stream, for example) */
	time_t		connect_epoch;		/* the time the socket was connected or, to be specific, when the task was created */
	int		ad_inject;
	int		request_end;		/* If we are an encoder, we are requested to stop */

	/*
	 * etag and identity are unique connection IDs (more or less) and they should be the same.
	 * session is the sessionid, as received from the cookie field.
	 */
	char		etag[64], 		
			identity[64],
			sessionid[256];


	/*
	 * The AU audio format parameters. This is needed for the encoder
	 */
	AUFormat au;

	
	pthread_mutex_t	lock;		/* Used to lock an individual thread of execution */
#if defined(INIT_BURST_FLASHBACK) && defined(INIT_BURST_ON_CONNECT)
	int	flashback;
	int	init;
#endif

	struct	st_Threads	*next;		/* this is a linked list */
} *pThreads, Threads;

extern	pThreads	AllThreadsHead;

extern	pThreads	new_task(void);
#ifdef CONSOLE_DEBUGGING
 extern  int		MemLock_Assert(char *, int);
#else
 extern	int		MemLock(void);
#endif
extern	int		MemUnlock(void);
extern	int		atomic(void);
extern	int		noatomic(void);

extern	void		add_to_rdb(pThreads, void *, int);
extern	void		task_remove(pThreads);
extern	void		task_finish(pThreads);
extern	int		task_count(void);
extern	int		task_lock(pThreads), task_unlock(pThreads);
extern	int		task_glock(void), task_gislocked(void), task_gunlock(void);
extern	int		task_lock_no_global_check(pThreads);
extern	int		task_string_status(pThreads, char *, int);
extern	pThreads	task_by_identity(const char *, int);

/*
** this doesn't work in distro.h. Kinda weird -- maybe some kind of gcc bug?
*/
extern  int     serve_from_identity(pThreads, const char *);

#endif

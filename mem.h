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

#define	SetStatus_Raw(s, num)	{ s->status = ((s->status & ~0xf) | num); }

#define	SetStatus_Begin(s)	SetStatus_Raw(s, SSTAT_BEGIN)
#define	SetStatus_WaitRQ(s)	SetStatus_Raw(s, SSTAT_WAITRQ)
#define	SetStatus_BadKey(s)	SetStatus_Raw(s, SSTAT_BADKEY)
#define	SetStatus_PreStream(s)	SetStatus_Raw(s, SSTAT_PRESTREAM)
#define	SetStatus_Stream(s)	SetStatus_Raw(s, SSTAT_STREAM)
#define	SetStatus_Finish(s)	SetStatus_Raw(s, SSTAT_FINISH)
#define	SetStatus_Dead(s)	SetStatus_Raw(s, SSTAT_DEAD)
#define SetStatus_Stats(s)	SetStatus_Raw(s, SSTAT_STATS)

#define	IsStatusWaitRQ(s)	((s->status & 0xf) == SSTAT_WAITRQ)
#define	IsStatusStream(s)	((s->status & 0xf) == SSTAT_STREAM)
#define IsStatusFinish(s)	((s->status & 0xf) == SSTAT_FINISH)
#define	IsStatusDead(s)		((s->status & 0xf) == SSTAT_DEAD)

#define IsAdvertising(s)	((s->ad_inject))
#define	SetAdvertising(s)	s->ad_inject=!0
#define	UnSetAdvertising(s)	s->ad_inject=0

typedef struct	st_Threads {
	pSocket		sock;
	pthread_t	handler;
	struct {
		void	*buf;
		size_t	si;
	} rdb;
	struct	{
		void	*buf;
		size_t	si;
	} wrb;

	pBytes	rbuf;

	void	*Extra;

	struct	st_Threads	*next;

	time_t	last;

	unsigned int	status;
	time_t		connect_epoch;		/* the time the socket was connected or, to be specific, when the task was created */
	int		ad_inject;

	char	sessionid[256];
} *pThreads, Threads;

extern	pThreads	AllThreadsHead;

extern	pThreads	new_task(void);
extern	int		MemLock(void);
extern	int		MemUnlock(void);
extern	int		atomic(void);
extern	int		noatomic(void);

extern	void		add_to_rdb(pThreads, void *, int);
extern	void		task_remove(pThreads);
extern	void		task_finish(pThreads);
extern	int		task_count(void);
extern	int		task_string_status(pThreads, char *, int);

#endif

#ifndef __SIGNAL_H__
#define __SIGNAL_H__

//typedef	struct	st_sigs {
//	
//} Sigs, *pSigs;

extern	int	signal_init(void);
extern	int	SigLock(void);
extern	int	SigUnlock(void);
extern	void	*signal_thread(void *);
extern	int	signal_if(unsigned int, int (*)(), int *);

#define	SIGNAL_INT	1	
#define SIGNAL_HUP	2
#define SIGNAL_TERM	4
#define SIGNAL_USR1	8
#define SIGNAL_USR2	16

#define	IsSignalINT(x)		((x) & SIGNAL_INT)
#define IsSignalHUP(x)		((x) & SIGNAL_HUP)
#define	IsSignalTERM(x)		((x) & SIGNAL_TERM)
#define	IsSignalUSR1(x)		((x) & SIGNAL_USR1)
#define	IsSignalUSR2(x)		((x) & SIGNAL_USR2)

#define	RaiseSignal(x, signum)	((x) |= signum)
#define LowerSignal(x, signum)	((x) &= ~signum)

#endif /* __SIGNAL_H__ */

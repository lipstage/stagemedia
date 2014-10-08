#ifndef __CONTROL_H__
#define	__CONTROL_H__

#include "stagemedia.h"

#define	IsCtrlAudio(s)		(s->type == SMPL_AUDIO)
#define	IsCtrlControl(s)	(s->type == SMPL_CONTROL)

#define SMPL_AUDIO 1
#define SMPL_CONTROL 2

typedef	struct	st_CtrlData {
	int	type;
	void	*data;
	int	dsize;
	int	ctflag;
} CtrlData, *pCtrlData;


extern	int	Ctrl_Primer(unsigned short int, char *, size_t);
extern	int	Ctrl_UnPrime(unsigned char *);

extern	pBytes	Ctrl_Raw(int, void *, unsigned short);
extern	pBytes	Ctrl_Datum(void *, unsigned short);
extern	int	Ctrl_UnPack(pCtrlData, void *, int);
extern	void	Ctrl_UnPack_Free(pCtrlData);
extern	void	Ctrl_Pack_Free(pBytes);


#endif

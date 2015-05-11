#ifndef __CONTROL_H__
#define	__CONTROL_H__

#include "stagemedia.h"

#define	IsCtrlAudio(s)		(s->type == SMPL_AUDIO)
#define	IsCtrlControl(s)	(s->type == SMPL_CONTROL)

#define	QUALITY_ACCEPTABLE	0
#define	QUALITY_GOOD		1
#define QUALITY_VERYGOOD	2

/*
#define	QualityAcceptable(options)	(((options >> 2) & 0xf) == QUALITY_ACCEPTABLE)
#define	QualityGood(options)		(((options >> 2) & 0xf) == QUALITY_GOOD)
*/


#define SMPL_AUDIO 1
#define SMPL_CONTROL 2

typedef	struct st_CtrlPrimerHeader {
	int	version;
	int	password_protected;
	int	compression_enabled;
	int	quality;
	char	salt[9];
} CtrlPrimerHeader, *pCtrlPrimerHeader;

typedef	struct	st_CtrlData {
	int	type;
	void	*data;
	int	dsize;
	int	ctflag;
} CtrlData, *pCtrlData;


extern	int	Ctrl_Primer(unsigned short int, char *, size_t, const char *, char *);
extern	int	Ctrl_UnPrime(unsigned char *, pCtrlPrimerHeader);

extern	pBytes	Ctrl_Raw(int, void *, unsigned short);
extern	pBytes	Ctrl_Datum(void *, unsigned short);
extern	int	Ctrl_UnPack(pCtrlData, void *, int);
extern	void	Ctrl_UnPack_Free(pCtrlData);
extern	void	Ctrl_Pack_Free(pBytes);


#endif

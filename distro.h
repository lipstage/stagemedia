#ifndef __DISTRO_H__
#define	__DISTRO_H__

#include "stagemedia.h"

extern	int	distro_init(void);
extern	int	SocketPush();
extern	int	PH_SocketPush();
extern	void    *ProcessHandler();
extern	void    *CleanUp();
extern	void    *GetFromMaster();

#endif

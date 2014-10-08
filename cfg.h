#ifndef	__CFG_H__
#define	__CFG_H__

#include "stagemedia.h"

typedef struct st_config {
	char	key[1024];
	char	value[1024];
	struct	st_config	*next;
} Config, *pConfig;

extern	void	read_config(void);
extern	void	add_config(const char *, const char *);
extern	const char	*cfg_read_key(const char *);
extern	const char	*cfg_read_key_df(const char *, const char *);

#endif /* __CFG_H__ */

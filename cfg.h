#ifndef	__CFG_H__
#define	__CFG_H__

#include "stagemedia.h"

#define	int_cfg_read_key_df(x, y)	(cfg_read_key(x) ? atoi(cfg_read_key(x)) : y)

typedef struct st_config {
	char	key[1024];
	char	value[1024];
	struct	st_config	*next;
} Config, *pConfig;

extern	int	read_config(const char *, int);
extern	void	add_config(pConfig *, const char *, const char *);
extern	void	purge_config(pConfig *);
extern	const char	*cfg_read_key(const char *);
extern	const char	*cfg_read_key_df(const char *, const char *);
extern	int	cfg_is_true(const char *, int);
extern	int	cfg_version(void);

#endif /* __CFG_H__ */

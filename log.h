#ifndef __LOG_H__
#define __LOG_H__

#define LOG_DEBUG2	2
#define LOG_DEBUG	3
#define	LOG_INFO	5
#define LOG_WARN	10
#define LOG_ERR		15
#define	LOG_CRITICAL	20
#define LOG_		255

#define	debug(...)	loge(LOG_DEBUG, ###__VA_ARGS__)
#define	debug2(...)	loge(LOG_DEBUG2, ##__VA_ARGS__)
#define	info(...)	loge(LOG_INFO, ##__VA_ARGS__)
#define warn(...)	loge(LOG_WARN, ##__VA_ARGS__)
#define err(...)	loge(LOG_ERR, ##__VA_ARGS__)
#define critical(...)	loge(LOG_CRITICAL, ##__VA_ARGS__)
#define b(...)		loge(LOG_, ##__VA_ARGS__)

extern void	log_init();
extern void	log_close();
extern void	loge(int, const char *, ...);
extern char	*log_level_name(char *, int, int);
extern int	log_should(const char *);

#endif 

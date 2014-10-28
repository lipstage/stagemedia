#ifndef __LOG_H__
#define __LOG_H__

#define LOG_DEBUG	1
#define	LOG_DEBUG2	2
#define	LOG_INFO	5
#define LOG_WARN	10
#define LOG_ERR		15
#define	LOG_CRITICAL	20
#define LOG_		255

extern void	log_init();
extern void	log_close();
extern void	loge(int, const char *, ...);
extern char	*log_level_name(char *, int, int);
extern int	log_should(const char *);

#endif 

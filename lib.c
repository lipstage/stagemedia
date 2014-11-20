#include "stagemedia.h"

void	mypause() {
	mypause_time(10);
}

void    mypause_time(int ms) {
	struct  timeval tv;
        
	tv.tv_sec = 0;
	tv.tv_usec = ms * 1000;
	select(1, NULL, NULL, NULL, &tv);
}

int	mypause_fd(int fd, int ms) {
	int	retval;
	fd_set  rdfs;
	struct  timeval tv;

        /*
	 * Let's wait for 250ms for a possible connection
         */
	FD_ZERO(&rdfs);
        FD_SET(fd, &rdfs);
        tv.tv_sec = 0;
        tv.tv_usec = ms * 1000;

        /*
	 * Test for data availability.  Return NULL if nothing
         */
	retval = select(fd+1, &rdfs, NULL, NULL, &tv);
	
	return retval;
}

/*
 * Uppercase a given string 
 */
char	*strupper(char *s) {
	char	*p = s;

	while (*s) {
		if (islower(*s))
			*s = toupper(*s);
		s++;
	}
	return p;
}

/*
 * Lowercase a given string 
 */
char	*strlower(char *s) {
	char	*p = s;

	while (*s) {
		if (isupper(*s)) 
			*s = tolower(*s);
		s++;
	}
	return p;
}

/*
 * Trim a string
 */
char *trim(char *s) {
	char	*p;
	int	len;

	/* trim off the back first */
	while ( (p = strrchr(s, '\r')) || (p = strrchr(s, '\n')) )
		*p = '\0';
 
	len = strlen(s);
	p = s;
	while (*p && isspace(*p)) 
		memmove(s, (p + 1), --len);
	
	for (p = s + strlen(p) -1; *s && isspace(*p); --p) 
		*p = '\0';
	
	return s;
}

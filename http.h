#ifndef	__HTTP_H__
#define	__HTTP_H__

#include "stagemedia.h"

#define	HTTP_VERB_GET	1
#define	HTTP_VERB_POST	2
#define	HTTP_VERB_PUT	3

#define	HTTP_RQ_00		0
#define	HTTP_RQ_STATS		99
#define	HTTP_RQ_INVALID		0xffff

#define	WouldBlock(r)	((r < 0) && (errno == EAGAIN || errno == EWOULDBLOCK) )
#define	SockError(r)	((r < 0))

typedef	struct	st_HTTPCode 
{
	int	code;
	int	clos;
	char	* phrase;
} HTTPCode;

/*
 * The data will be parsed from a request and thrown into the structure
 * below.
 *
 * GET /{{file_path}}/{{options}}
 * Cookie: sessionid={{cookie_string}}; ...
 */
typedef struct st_Request {
	int	verb;
	char	uri[1024];
	char	file_path[1024];	
	char	cookie_string[1024];		/* we only care about sessionid={{string}} */
} HTTPRequest;

extern	HTTPCode	http_code_get(int);
extern	int		http_send_header(pSocket, int, char *, char *);
extern	int		http_fetch_request(pSocket, int *, char *);
extern	int		http_filepath(const char *, char *, size_t);

#endif 

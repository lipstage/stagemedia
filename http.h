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

typedef	struct	st_HTTPCode {
	int	code;
	int	clos;
	char	* phrase;
} HTTPCode;

typedef	struct	st_HTTPHeaders {
	char	name[256];
	char	value[4096];
	struct	st_HTTPHeaders	*next;
} HTTPHeaders, *pHTTPHeaders;

typedef	struct	st_HTTPHeader {
	pHTTPHeaders	start;
	int		error;
} HTTPHeader, *pHTTPHeader;

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
	char	sessionid[1024];
	char	user_agent[4096];
	struct {
		int	min;
		int	max;
	} range;
	int	use_range;
	char	lipstage_session[1024];
} HTTPRequest;

extern	HTTPCode	http_code_get(int);
extern	int		http_send_header(pSocket, int, char *, char *, pHTTPHeader);
extern	int		http_fetch_request(pSocket, int *, char *, HTTPRequest *);
extern	int		http_filepath(const char *, char *, size_t);
extern	int		wheader(pSocket, const char *, ...);
extern	pHTTPHeader	http_header_init(void);
extern	int		http_header(pHTTPHeader, const char *, const char *, ...);
extern	void		http_header_clear(pHTTPHeader);
#endif 

#include "stagemedia.h"

/*
 * Fetch the HTTP request from the client for processing.
 */
int	http_fetch_request(pSocket sock, int *method, char *sessionid, HTTPRequest *request_info) {
	int	count = 0;
	char	buffer[16384] = { 0 };
	HTTPRequest	rq;
	int	term = 0, ret;
	time_t	st_time = time(0);

	rq.use_range = 0;
	memset(rq.lipstage_session, 0, sizeof rq.lipstage_session);

	while ((ret = sock_readline(buffer, sizeof buffer, sock))) {
		char    *p = NULL;

		/* if we received an error, then return -1 */
		if (ret < 1 || ( (time(0) - st_time) > HTTP_CLIENT_MAX_TIME))
			return -1;

		while ((p = strchr(buffer, '\r')))
			*p = '\n';
		while ((p = strchr(buffer, '\n')))
			*p = '\0';

		/* show what we got in the header */
		loge(LOG_DEBUG2, "http.headers (fd=%d): >> %s", sock->fd, buffer);

		if (*buffer != '\0' && !term) {
			/*
			 * Getting the beginning, like 'GET / HTTP/1.1'
			 */
			if (count == 0) {
				char    *tok[64];
				int     i;

				for (i = 0, tok[i] = buffer, tok[i] = strtok(tok[i], " ");
					i < 64 && tok[i];
					tok[++i] = strtok(NULL, " "));

				if (!strcasecmp(tok[0], "get") && i >= 3) {
					rq.verb = HTTP_VERB_GET;
					strncpy(rq.uri, tok[1], sizeof rq.uri - 1);
					http_filepath(rq.uri, rq.file_path, sizeof rq.file_path - 1);
	
					if (!strncmp(rq.file_path, "00", 2)) {
						*method = HTTP_RQ_00;
					} else if (!strncmp(rq.file_path, "99stats", 6)) {
						*method = HTTP_RQ_STATS;
					} else {
						term = 404;
						continue;
					} 
				} else {
					term = 400;
					continue;
				}
			} else {
			/*
			 * All other fields, we DO want a cookie!
			 */
				char	*field, *after, *tmp;
				char	*cstring;

				/* So the big dog doesn't care about sessionid, why should we */
				if (!sessionid)
					continue;
				else if (!*sessionid) {
					/* The cookie CAN be provided via the URI path in some cases, so we use that as a priority */
					if ((cstring = strrchr(rq.file_path, '_'))) {
						cstring++;
						if (*cstring && strlen(cstring) > 6) {
							strncpy(sessionid, cstring, 1023);
							//continue;
						}
					}
				}

				/* Looking for colon as the separator at this point */
				if (!(field = strchr(buffer, ':')))
					continue;

				/* clip the name off, we want cookie :-) */
				*field = '\0';


				if (!strcasecmp(buffer, "User-Agent")) {
					after = trim(field + 1);
					if (after && *after)
						strncpy(rq.user_agent, after, sizeof rq.user_agent - 1);

					continue;
				}

				if (!strcasecmp(buffer, "X-Lipstage-Session") || !strcasecmp(buffer, "if-range")) {
					after = trim(field + 1);
					remove_chars(after, '"');
					remove_chars(after, '\'');
					if (after && strlen(after) >= IDENTITY_STRLEN) {
						strncpy(rq.lipstage_session, after, sizeof rq.lipstage_session);
					}
					continue;
				}

				/* If we received a "range" */
				if (!strcasecmp(buffer, "Range")) {
					char	range[1024];
					char	*left, *right;
					int	range_min = -1, range_max = -1;
	
					after = field + 1;
				
					while(*after && isspace(*after))
						after++;

					/* expecting that bytes= */
					if (strncmp(after, "bytes=", strlen("bytes=")))
						continue;
	
					/* we expect to see 'bytes=' */
					strncpy(range, &after[strlen("bytes=")], sizeof range - 1);

					/* get the actual range of bytes */
					if (!(tmp = strchr(range, '-'))) 
						continue;
					left = range;
					right = tmp + 1;
					*tmp = '\0';

					/* probably shouldn't be using atoi for this, but... */
					if (strlen(left)) 
						range_min = atoi(left);
					if (strlen(right))
						range_max = atoi(right);

					/* bad range */
					if (range_min < 0 && range_max < 0)
						continue;

					/* we are using the range */
					rq.use_range = 1;
					rq.range.min = range_min;
					rq.range.max = range_max;

					continue;
				}

				/* we SPECIFICALLY want cookie! */
				if (strcasecmp(buffer, "Cookie"))
					continue;

				/* Get what is after the cookie */
				after = field + 1;
			
				/* Get rid of any whitespace */
				while (*after && isspace(*after))
					after++;

				/* If it is an identity, we might need it ;) */
				if (!strncmp(after, "identity=", strlen("identity="))) {
					char	*p = after + strlen("identity=");
				
					remove_chars(p, '"');
					remove_chars(p, '\'');
					if (p && *p && !*rq.lipstage_session && strlen(p) >= IDENTITY_STRLEN)
						strncpy(rq.lipstage_session, p, sizeof rq.lipstage_session);
					continue;
				}

				/* Should be sessionid=<cookie> -- if it isn't, then we restart the loop */
				if (strncmp(after, "sessionid=", strlen("sessionid=")))
					continue;

				/* We should get a semi-colon with something after it, we just ignore that piece, don't care */
				if ((tmp = strchr(after, ';')))
					*tmp = '\0';

				/* the sessionid needs to be so big */
				if (strlen(after) < (strlen("sessionid=")+10))
					continue;

				/* copy over the cookie!  Ok, this is a little gross, could be cleaned up later :P */
				strncpy(sessionid, &after[ strlen("sessionid=") ], 1023);
				strncpy(rq.sessionid, sessionid, sizeof rq.sessionid - 1);
			}
		} else if (!*buffer) {
			break;
		}
		count++;
	}

	if (term) {
		http_send_header(sock, term, NULL, NULL, NULL);
	}

	/* copy over the HTTPRequest information (if asked) */
	if (request_info) 
		memcpy(request_info, &rq, sizeof rq);
	return term;
}

int	http_filepath(const char *path, char *output, size_t len) {
	int	count = 0;

	while (*path == '/' && *path != 0)
		path++;

	while (*path != 0 && *path != '/') {
		if (count >= len)
			break;
		output[count++] = *path;
		path++;
	}
	output[count] = '\0';
	return count;
}

pHTTPHeader	http_header_init(void) {
	pHTTPHeader	new;

	if (!(new = calloc(1, sizeof *new)))
		return NULL;
	new->start = NULL;
	new->error = 0;
	return new;
}

/*
 * http_send_header - Send the HTTP headers to the client who is connected.
 *
 * sock = The socket object we're going to use.
 * code = The actual code, such as 200, 404, 500, etc.
 * content_type = What "Content-Type" should say to the client.  If this is NULL, it will be
 *	text/plain
 */
int	http_send_header(pSocket sock, int code, char *content_type, char *etag, pHTTPHeader header) {
	HTTPCode c;

	c = http_code_get(code);
	if (c.phrase == NULL)
		c = http_code_get(500);

	wheader(sock, "HTTP/1.1 %d %s\r\n", c.code, c.phrase);
	wheader(sock, "Server: %s/%s\r\n", HTTP_SERVER_NAME, HTTP_SERVER_VERSION);
	wheader(sock, "Content-Type: %s\r\n", content_type ? content_type : "text/plain");
	//wheader(sock, "Cache-Control: no-cache, must-revalidate\r\n");
	//wheader(sock, "Pragma: no-cache\r\n");
	//wheader(sock, "Expires: -1\r\n");
	
	/* Send any custom headers if necessary */
	if (header) {
		pHTTPHeaders	scan;

		for (scan = header->start; scan; scan = scan->next) {
			char	buffer[8192];

			snprintf(buffer, sizeof buffer - 1, "%s: %s\r\n", scan->name, scan->value);
			wheader(sock, buffer);
		}
		http_header_clear(header);
	}

	if (etag) {
		wheader(sock, "Set-Cookie: identity=%s\r\n", etag);
		wheader(sock, "ETag: \"%s\"\r\n", etag);		
	}


	wheader(sock, "Connection: close\r\n");

	/* write very last part */
	wheader(sock, "\r\n");

	/* If it says close, we clos -- or we say to */
	if (c.clos) {
		return 1;
	}
		
	return 0;
}

/*
 * Add header as a name:value par in a linked list to be used
 * with http_send_header later
 */
int	http_header(pHTTPHeader header, const char *name, const char *value, ...) {
	pHTTPHeaders	temp;
	va_list		ap;

	if (!(temp = calloc(1, sizeof *temp))) {
		header->error++;
		return !0;
	}
	temp->next = NULL;

	va_start(ap, value);
	vsnprintf(temp->value, sizeof temp->value - 1, value, ap);
	va_end(ap);
	strncpy(temp->name, name, sizeof temp->name - 1);

	if (!header->start) 
		header->start = temp;
	else {
		pHTTPHeaders	scan;

		for (scan = header->start; scan->next; scan = scan->next)
			;
		scan->next = temp;
	}
	return 0;
}

/*
 * Clear out the memory from http_header()
 */
void	http_header_clear(pHTTPHeader header) {
	while (header->start) {
		pHTTPHeaders	p;
		
		p = header->start->next;
		free (header->start);
		header->start = p;
	}
	free (header);
}

HTTPCode http_code_get(int code) {
	HTTPCode data[] = {
		{	200,	0,	"OK"					},
		{	206,	0,	"Partial Content"			},
		{	403,	0,	"Permission Denied"			},
		{	404,	1,	"Not Found"				},
		{	500,	1,	"Server Error"				},
		{	400,	1,	"Bad Request"				},
		{	416,	0,	"Requested Range Not Satisfiable"	},
		{	0,	0,	NULL					}
	};
	int	i;

	for (i = 0; data[i].phrase; i++) 
		if (code == data[i].code)
			return data[i];
	return data[i];
}

int	wheader(pSocket sock, const char *fmt, ...) {
	char	buffer[4096];
	va_list	ap;

	va_start(ap, fmt);
	vsnprintf(buffer, sizeof buffer, fmt, ap);
	va_end(ap);

	loge(LOG_DEBUG2, "http.headers (fd=%d): << %s", sock->fd, buffer);

	return write(sock->fd, buffer, strlen(buffer));
}

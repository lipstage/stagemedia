#include "stagemedia.h"

/*
 * Fetch the HTTP request from the client for processing.
 */
int	http_fetch_request(pSocket sock, int *method, char *sessionid) {
	int	count = 0;
	char	buffer[16384] = { 0 };
	HTTPRequest	rq;
	int	term = 0, ret;
	time_t	st_time = time(0);

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

				/* The cookie CAN be provided via the URI path in some cases, so we use that as a priority */
				if ((cstring = strrchr(rq.file_path, '_'))) {
					cstring++;
					if (*cstring && strlen(cstring) > 6) {
						strncpy(sessionid, cstring, 1023);
						continue;
					}
				}
				
				/* Looking for colon as the separator at this point */
				if (!(field = strchr(buffer, ':')))
					continue;

				/* clip the name off, we want cookie :-) */
				*field = '\0';

				/* we SPECIFICALLY want cookie! */
				if (strcasecmp(buffer, "Cookie"))
					continue;

				/* Get what is after the cookie */
				after = field + 1;
			
				/* Get rid of any whitespace */
				while (*after && isspace(*after))
					after++;

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
			}
		} else if (!*buffer) {
			break;
		}
		count++;
	}

	if (term) {
		http_send_header(sock, term, NULL, NULL);
	}
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

/*
 * http_send_header - Send the HTTP headers to the client who is connected.
 *
 * sock = The socket object we're going to use.
 * code = The actual code, such as 200, 404, 500, etc.
 * content_type = What "Content-Type" should say to the client.  If this is NULL, it will be
 *	text/plain
 */
int	http_send_header(pSocket sock, int code, char *content_type, char *etag) {
	HTTPCode c;
	char	buffer[65535];

	c = http_code_get(code);
	if (c.phrase == NULL)
		c = http_code_get(500);
	
	snprintf(buffer, sizeof buffer - 1, 
		"HTTP/1.1 %d %s\r\n"
		"Server: %s/%s\r\n"
		"Cache-Control: no-cache\r\n"
		"Pragma: no-cache\r\n"
		"Expires: -1\r\n"
		"Connection: close\r\n"
		"Content-Type: %s\r\n"
		"%s%s%s"
		"\r\n",
		c.code, c.phrase, HTTP_SERVER_NAME, HTTP_SERVER_VERSION, content_type ? content_type : "text/plain",
		etag ? "Etag: ": "", etag ? etag : "", etag ? "\r\n" : ""
	);

	/* write to the socket right now :) */
	write(sock->fd, buffer, strlen(buffer));

	/* log what we sent to the client */
	loge(LOG_DEBUG2, "http.headers (fd=%d): << %s", sock->fd, buffer);

	/* If it says close, we clos -- or we say to */
	if (c.clos) {
		return 1;
	}
		
	return 0;
}


HTTPCode http_code_get(int code) {
	HTTPCode data[] = {
		{	200,	0,	"OK"			},
		{	403,	0,	"Permission Denied"	},
		{	404,	1,	"Not Found"		},
		{	500,	1,	"Server Error"		},
		{	400,	1,	"Bad Request"		},
		{	0,	0,	NULL			}
	};
	int	i;

	for (i = 0; data[i].phrase; i++) 
		if (code == data[i].code)
			return data[i];
	return data[i];
}


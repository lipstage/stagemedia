#include "stagemedia.h"


/*
 * Bind to a specific address and port
 */
int	bind_address(const char *theaddr, ushort port) {
	int s, yes = 1;
	struct sockaddr_in addr;

	/* create the socket */
	s = socket(PF_INET, SOCK_STREAM, 0);

	/* Zero out everything -- probably not needed on Linux */
	memset(&addr, 0, sizeof(addr));

	/* IPv4 */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port); 
	addr.sin_addr.s_addr = inet_addr(theaddr);

	/* Set the socket to be reusable -- assume it works */
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	/* bind to the socket */
	if (bind(s, (struct sockaddr *)&addr, sizeof addr) < 0) {
		perror("bind");
		return -1;
		//exit(-1);
	}

	/* start the listening! */
	if (listen(s, 100) < 0) {
		perror("listen");
		//exit(-1);
		return -1;
	}

	/* throw back the fd! */
	return s;
}

/*
 * Accept a new socket
 */
pSocket	sock_accept(int fd) {
	int	s;
	unsigned int	si;
	struct sockaddr_in	addr;

	/* Wait up to 1 second for a change in the socket (maybe a connection!) */
	if ((mypause_fd(fd, 1000)) <= 0)
		return NULL;

	/*
	 * Accept the new connection (if available) and store the IP information
	 * and other parameters as a native sockaddr_in
	 */
	si = sizeof(addr);
	if ((s = accept(fd, (struct sockaddr *)&addr, &si)) > 0) {
		pSocket	new;

		/* Should probably handle this better */
		if (!(new = calloc(1, sizeof(*new))))
			return NULL;

		/* copy the socket over */
		memcpy(&new->sin, &addr, sizeof(new->sin));

		memset(new->ip_addr_string, 0, sizeof new->ip_addr_string);
		sprintf(new->ip_addr_string, "%d.%d.%d.%d", 
			(new->sin.sin_addr.s_addr & 0xff),
			(new->sin.sin_addr.s_addr >> 8) & 0xff,
			(new->sin.sin_addr.s_addr >> 16) & 0xff,
			(new->sin.sin_addr.s_addr >> 24) & 0xff
		);
		sprintf(new->port_string, "%u",
			ntohs(new->sin.sin_port));

		/* set the fd */
		new->fd = s;

		/* we start at zero */
		new->bpos = 0;

		/* socket buffer is 0 */
		new->in_buf = NULL;

		/* Set as a client */
		new->flag |= SOCK_FLAG_CLIENT;

		/* log the connection */
		loge(LOG_INFO, "(fd: %d) Connection established from host %s:%s", new->fd, new->ip_addr_string, new->port_string);

		/* And...we have a connection! */
		return new;
	}
	return NULL;
}

/*
 * Read data into the socket's buffer
 */
int	sock_read_into_buffer(pSocket s, int maxsize) {
	int	si;
	unsigned char	buffer[16384];

	/* The socket/connection is dead -- we can no longer read from it */
	if (IsDeadSocket(s)) {
		if (!s->bpos) {
			/* This is probably zero or NULL */
			if (s->in_buf != NULL) 
				free(s->in_buf);
			return -1;
		} else 
			return 0;	/* do not cry wolf just yet */
	}

	/* If the overall buffer is too big, then abort */
	if (s->bpos >= MAXBUFFER)
		return 0;
	
	/* force good sizing */
	maxsize = maxsize > sizeof buffer ? sizeof buffer : maxsize;

	/* If it is less than 16K, then be willing to read more :) */
	if (maxsize < 16384)
		maxsize = 16384;

	/* read the necessary data! */
	//si = read(s->fd, buffer, maxsize);
	si = recv(s->fd, buffer, maxsize, MSG_DONTWAIT);

	/* If there is some kind of error... */
	if ((si < 0 && errno != EAGAIN && errno != EWOULDBLOCK) || si == 0) {
		s->deadsocket = errno;
		return sock_read_into_buffer(s, maxsize);
	} 

	/* See if we actually read anything */
	if (si == 0) {
		return 0;
	} else if (si < 0) {
		/* something happened, we need more error checking here */
		return 0;
	}

	if (s->bpos == 0) {
		/* size accordingly */
		if (!(s->in_buf = calloc(1, si))) {
			loge(LOG_ERR, "sock_read_into_buffer: Out of memory!");
			return -1;			
		}

		/* copy the data over */
		memcpy(s->in_buf, buffer, si);

		/* set bpos */
		s->bpos = si;

	} else {
		unsigned char	*p;

		/* attempt the realloc.  Technically, if in_buf was NULL, we could do realloc(NULL, size) -- but eh [doesn't zero] */
		if (!(p = realloc(s->in_buf, s->bpos + si)))
			; /* It failed -- should handle, lol */

		if (s->in_buf != p)
		/* we have a (potentially) new address  */
		s->in_buf = p;

		/* copy it over, yay! */
		memcpy(&s->in_buf[s->bpos], buffer, si);

		/* update bpos */
		s->bpos += si;
	}

	/* return the size */
	return si;
}


/*
 * Get the data out from the existing buffer.  If we have nothing in the buffer, 
 * then we proceed to try and read more data into the buffer.
 */
int	sock_read_from_buffer(pSocket s, int maxsize, void *data) {

	/*
	 * Make an effort to get maxsize (if one can)
	 */
	if (s->bpos < maxsize) {
		int ret = 0;

		ret = sock_read_into_buffer(s, maxsize);
		if (s->bpos <= 0 && IsDeadSocket(s))
			return ret;
	}

	/*
	 * We can proceed!
	 */
	if (s->bpos > 0) {
		int si, new_size;

		/* get the "true" appropriate size :) */
		si = maxsize > s->bpos ? s->bpos : maxsize;

		/* pull the data out */
		memcpy(data, s->in_buf, si);

		/* this will be the new size */
		new_size = s->bpos - si;

		/* If the size is nothing, then we can free everything! */
		if (new_size == 0) {

			/* free it */
			free (s->in_buf);

			/* be sure to set it to NULL for later goodness */
			s->in_buf = NULL;

			/* And there's no size now! :) */
			s->bpos = 0;

		} else {
			unsigned char	*p;

			/* MATH :( */

			/* VERY important that we move memmove to copy memory -- the addresses CAN overlap! */
			memmove(s->in_buf, &s->in_buf[si], new_size);

			/* Update the size now */
			s->bpos = new_size;

			/* Time to realloc -- this shouldn't fail -- VERY low risk here */
			if (!(p = realloc(s->in_buf, new_size)))
				;	/* WTF */

			s->in_buf = p;
		}

		/* return the actual number of bytes copied */
		return si;
	}

	/* Nothing */
	return 0;
}


/*
 * sock_read - Read data from the input buffer and fill it, if necessary.
 *
 * Surprise!  This is more or less sock_read_from_buffer :)
 */
int sock_read(pSocket s, int maxsize, void *data) {
	return sock_read_from_buffer(s, maxsize, data);
}


/*
 * sock_close - Time to close the socket
 */
void sock_close(pSocket s) {
	/* we received nothing */
	if (!s)
		return;

	/* If there's "buffered" data we are using here */
	if (s->bpos && s->in_buf)
		free (s->in_buf);

	/* close socket */
	close (s->fd);

	/* free the memory */
	free (s);
}


/*
 * Connect to a remote server
 */
pSocket	sock_connect(const char *where, unsigned short uport) {
	struct	addrinfo	hints, *res;
	char	port[64];
	int	fd;

	snprintf(port, sizeof port - 1, "%u", uport);	

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* Did the call happen to error out? */
	if ((getaddrinfo(where, port, &hints, &res)))
		return NULL;

	/* Try to grab a socket that may be available */
	if ((fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
		return NULL;

	/* Now, we connect! */
	if (!(connect(fd, res->ai_addr, res->ai_addrlen))) {
		pSocket	new;

		if ((new = calloc(1, sizeof *new))) {

			/* Set the file descriptor */
			new->fd = fd;

			/* Set the flag of being a Client */
			new->flag |= SOCK_FLAG_SERVER;

			/* buffer is 0 length */
			new->bpos = 0;

			return new;
		}
	}

	close (fd);
	return NULL;
}

int	sock_readline(char *buffer, size_t len, pSocket s) {
	char	c;
	int	ret = 0, place = 0;
	time_t	st_time = time(0);

	if (len <= 0)
		return -1;

	sock_nonblock(s);

	//while ((ret = read(s->fd, &c, 1)) > 0) {
	for (;;) {
		/* If we have gone above HTTP_CLIENT_MAX_TIME, then we return -1, simple as that. */
		if ( (time(0) - st_time) > HTTP_CLIENT_MAX_TIME)
			return -1;

		/* attempted to read 1 character -- this is NOT efficient, but it is brief */
		ret = read(s->fd, &c, 1);

		/*
		 * If we would block, then we wait up to 500ms for the socket
		 * to be "ready"
		 */
		if (WouldBlock(ret)) {
			/* pause/wait on the socket for 500ms */
			mypause_fd(s->fd, 500);

			/* start over */
			continue; 
		/*
		 * If the socket errored out, then we just return -1.
		 *
		 * It is expected that the caller will then close the socket and terminate
		 * the connection.
		 */
		} else if (SockError(ret) || !ret) 
			return -1;

		buffer[place++] = c;
		if (c == '\n' || place >= len) {
			sock_block(s);
			return place;
		}  
	}

	sock_block(s);
	return ret;
}

int	sock_nonblock(pSocket s) {
	int	flags = fcntl(s->fd, F_GETFL, 0);

	if (flags < 0)
		return 0;
	flags |= O_NONBLOCK;
	fcntl(s->fd, F_SETFL, flags);
	return !0;
}

int	sock_block(pSocket s) {
	int	flags = fcntl(s->fd, F_GETFL, 0);

	if (flags < 0) 
		return 0;
	flags &= ~O_NONBLOCK;
	fcntl(s->fd, F_SETFL, flags);
	return !0;
}

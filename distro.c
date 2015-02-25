#include "stagemedia.h"

int	distro_init(void) {
	int	serverfd;
	pthread_t	cleanup, frommaster;

	/* Attempt to bind to the address we'll use for HTTP/streaming */
	if ((serverfd = bind_address(cfg_read_key_df("distro_bind_ip", DISTRO_BIND_IP), int_cfg_read_key_df("distro_bind_port", DISTRO_BIND_PORT))) < 0) {
		loge(LOG_, "Unable to bind to desginated ip/port and cannot continue (binding=%s:%d)",
			cfg_read_key_df("distro_bind_ip", DISTRO_BIND_IP), int_cfg_read_key_df("distro_bind_port", DISTRO_BIND_PORT));
		exit(-1);
	}

	for (;;) {
		/* start pulling data from the master server */
		pthread_create(&frommaster, NULL, GetFromMaster, NULL);

		/* this is the cleanup job which removes old, stale, or dead connections */
		pthread_create(&cleanup, NULL, CleanUp, NULL);

		/* give back the serverfd! */
		return (serverfd);
	}
	return -1;
}



void	*CleanUp() {
	int	nopause = 0;

	for (;;) {
		pThreads	scan;

		/* pause up to 500ms */
		if (!nopause)
			mypause_time(500);

		/* Excusively lock the memory, we have stuff to do */
		MemLock();

		/* Loop through all tasks */
		for (scan = AllThreadsHead, nopause = 0; scan; scan = scan->next) {
			/*
			 * The task/job is complete, we just need to finish it out
			 */
			if (IsStatusFinish(scan)) {
				/* free the memory! */
				MemUnlock();

				/* Join until exit -- should not take very long at all */
				(void) pthread_join(scan->handler, NULL);

				/* Mark it as dead */
				SetStatus_Dead(scan);

				/* Lock the memory again -- we WANT to keep using this loop */
				MemLock();
			}

			/*
			 * If we have a ``dead'' job, we need to remove it.  Once we do that, 
			 * we will just have to restart the loop, more or less.
			 */
			if (IsStatusDead(scan)) {
				/* remove it in the linked list */
				task_remove(scan);

				/* We *CANNOT* proceed here, so we need to restart... set nopause = !0 */
				nopause = !0;

				/* leave the loop */
				break;
			}
		}
	
		MemUnlock();		
	}
}


/*
 * Each connection becomes a 'thread' in which the user/connection executes.
 * The 'ProcessHandler' function is actually one of the thread managers (mini-process)
 */
void	*ProcessHandler(pThreads s) {
	EncMP3	encode;
	AUFormat	au;
#ifdef	ALLOCATE_AUDIO_ENCODING_BUFFER
	int		audio_size = 0;
	short	int	*audio = NULL;
#else
	short	int	audio[MAX_TRANSCODE_CLIP_SIZE + sizeof(short int)] = { 0 };
#endif
	int	method = 0;
	int	init_stream = 0;	/* used locally to indicate we've started a stream */
	int	max_stream_time = cfg_read_key("max_stream_time") ? atoi(cfg_read_key("max_stream_time")) : MAX_STREAM_TIME;
	/* time_t	st_time = time(0); */
	char	sessionid[1024];

	/* set the connect time, NOW */
	s->connect_epoch = time(0);

	/* zero out the sessionid */
	memset(s->sessionid, 0, sizeof s->sessionid);

	/* debug entry to track when it started */
	loge(LOG_DEBUG, "(fd: %d, %s:%s) Client connected and distribution thread kicked off at %lu epoch",
		s->sock->fd, s->sock->ip_addr_string, s->sock->port_string, s->connect_epoch);

	/* set up au */
	au.channels = 1;
        au.data_offset = -1;
        au.data_size = -1;
        au.encoding = 3;
        au.sample_rate = 22050;

	/* Tell other processes that we're starting off */
	SetStatus_Begin(s);

	/* The time of 'last' big change */
	s->last = time(0);

	/* Waiting for request completion */
	SetStatus_WaitRQ(s);

	sock_block(s->sock);

	/* Get data about the http request */
	if (http_fetch_request(s->sock, &method, sessionid)) {
		loge(LOG_DEBUG, "(fd: %d, %s:%s) http_fetch_request returned non-zero, closing task.",
			s->sock->fd, s->sock->ip_addr_string, s->sock->port_string);
		task_finish(s);		/* exit */
	}

	/*
	 * If we are requesting stats, then it is somewhat simple...
	 */
	if (method == HTTP_RQ_STATS) {
		pThreads	scan;
		int		count = 0;
		char		buffer[8192];	

		/* Send the plain/text header, and, if not, close the socket and stuff */
		if (http_send_header(s->sock, 200, "text/plain"))
			task_finish(s);

		/* Let it know we are gathering stats */
		SetStatus_Stats(s);

		/* Lock the memory to prevent changes */
		MemLock();

		snprintf(buffer, sizeof buffer - 1, "V %s\r\n", STAGEMEDIA_VERSION);
		SocketPush(s, buffer, strlen(buffer));
 
		for (scan = AllThreadsHead; scan; scan = scan->next) {
			char	status_name[512];
			time_t	seconds_connected = 0;

			/* get the status of the connected process/thread/socket - ish */
			task_string_status(scan, status_name, sizeof status_name);

			/* number of seconds connected */
			seconds_connected = ( time(0) - scan->connect_epoch );

			/* this is the format we're going to push out */
			snprintf(buffer, sizeof buffer - 1, "C %d: ip_addr=%s,status=%s,con_sec=%lu,pcm_buf=%d,sessionid=%s\r\n", 
				++count, 
				scan->sock ? scan->sock->ip_addr_string : "N/A",
				status_name,
				seconds_connected,
				bytes_size(scan->rbuf),
				!*scan->sessionid ? "" : scan->sessionid
			);
			SocketPush(s, buffer, strlen(buffer));
		}

		/* Unlock the memory now, ok :) */
		MemUnlock();

		/* close the socket now */
		task_finish(s);
	}

	/* We have too many connections */
	if (cfg_read_key("max_clients") && atoi(cfg_read_key("max_clients")) <= task_count()) {
		if (http_send_header(s->sock, 500, NULL)) 
			task_finish(s);
	}

	/* user must present a valid cookie -- with login */
	if (cfg_is_true("api_session_check", 1)) {

		/* Invalid sesion.  The API says "no" to this */
		if (*sessionid == 0 || (valid_session(sessionid) <= 0)) {
			http_send_header(s->sock, 403, "text/plain");
			task_finish(s);
			loge(LOG_INFO, "Denying connection (fd: %d) from %s:%s due to invalid session",
				s->sock->fd, s->sock->ip_addr_string, s->sock->port_string);				
		}

		/* Log that we've authenticated the session */
		loge(LOG_INFO, "Accepting connection (fd: %d) from %s:%s with session id = %s",
			s->sock->fd, s->sock->ip_addr_string, s->sock->port_string, sessionid);

		/* Copy in the sessionid */
		strncpy(s->sessionid, sessionid, sizeof (s->sessionid) - 1);
	}

	/* Send the HTTP header */
	if (http_send_header(s->sock, 200, "audio/mpeg"))
		task_finish(s);		/* exit */

	/* We will now accept the PCM data */
	SetStatus_Stream(s);
	
	/* update the time for last 'big' change again */
	s->last = time(0);

	/* Get the encoder ready son */
	encode = encode_mp3_init(au);

	for (;;) {
		int		mcopy;
		int		maxamount;

		/* Lock the memory for thread-safe */
		MemLock();

		/*
		if (st_time > 0) {
			if ( (time(0) - st_time) >= 30 ) {
				st_time = 0;
				inject_advertising(s);
			}
		}
		*/

		/*
		 * If there are no bytes to read, then we proceed
		 * to wait 50ms, then restart our loop.  We do this
		 * until their are bytes
		 */
		if (!bytes_ready(s->rbuf)) {
			/* Unlock the memory area -- allow someone else to use it */
			MemUnlock();

			/* If we haven't received data beyond our time, then it is time to disconnect */
			if ((!init_stream && max_stream_time >= 0) && ((time(0) - s->last) >= max_stream_time)) {
				/* close the stream */
				encode_mp3_close(encode);

				/* write to the log */
				loge(LOG_INFO, "(fd: %d, %s:%s) Disconnecting client as we failed to start streaming within %d seconds", 
					s->sock->fd, s->sock->ip_addr_string, s->sock->port_string, max_stream_time);
				
				/* and, done */
				task_finish(s);
			}

			/* wait up to 50ms */
			mypause_time(50);

			/* restart */
			continue;
		}

		/* internally mark that we've received data */
		init_stream = 1;

		maxamount = bytes_size(s->rbuf);
		if (maxamount > MAX_TRANSCODE_CLIP_SIZE)
			maxamount = MAX_TRANSCODE_CLIP_SIZE;

		/* no sense in re-allocating, well, unless, of course, we HAVE to :) */
#ifdef ALLOCATE_AUDIO_ENCODING_BUFFER
		if (maxamount > audio_size) {
			free (audio);
			audio = NULL;
			audio_size = 0;
		} else 
			memset(audio, 0, audio_size);

		/* If audio is null, then we'll try to allocate for it.  If we can't, then, well, we die */		
		if (!audio) {
			if (!(audio = calloc(2, maxamount / 2))) {
				/* finish up encoding */
				encode_mp3_close(encode);

				/* our task is over :( */
				task_finish(s);
			} else
				audio_size = maxamount;
		}
#else
		memset(audio, 0, sizeof audio);
#endif

		/* calculate the squeeze */
		mcopy = (maxamount / sizeof(short int)) * sizeof(short int);

		if ( (mcopy % 2) != 0) {
			loge(LOG_WARN, "ProcessHandler: modulus operator expected 0, but is not in fd=%d, %s:%s",
				s->sock->fd, s->sock->ip_addr_string, s->sock->port_string);
		}

		/* grab the data */
		bytes_extract(s->rbuf, audio, mcopy);

		/* Unlock the memory now */
		MemUnlock();
			
		/* Looks like this bus is done */
		if ((encode_mp3_data(encode, audio, mcopy / 2, PH_SocketPush, s)) < 0) {
#ifdef	ALLOCATE_AUDIO_ENCODING_BUFFER
			/* free the temporary audio buffer */
			free (audio);
#endif
			/* We are going to close the encoder, gracefully */
			encode_mp3_close(encode);

			/* we now exit */
			task_finish(s);
		}

		//free (audio);
	}

	/* we're done! */
	task_finish(s);
	
	return NULL;		/* UNREACHABLE */
}

int	SocketPush(pThreads s, unsigned char *data, int size) {
	sock_block(s->sock);
	return write(s->sock->fd, data, size);
}

int	PH_SocketPush(pThreads s, unsigned char *data, int size) {
	int	nr;

	sock_nonblock(s->sock);
	nr = write(s->sock->fd, data, size);
	sock_block(s->sock);

	if (nr < 0 && !WouldBlock(nr)) 
		return -1;
	return nr;
}	

/*
 * Get data from the master server.
 */
void *GetFromMaster() {
	pSocket	m;
	pThreads	scan;
	int	count;
	unsigned	char	buffer[65535];
	pBytes		input_buffer = NULL;
	int		execone = 0;
	int		min_buffer = trans_buf_min(5, 1, 22050, 2);
			/* 5 second buffering time, 1 channel, 22050 samples/sec and 2 bytes (16-bit) per sample */
	MSDiff		diff;

	/* 0 the diff variable */
	memset(&diff, 0, sizeof diff);

	for (;;) {

		loge(LOG_INFO, 
			"Initiating connection to master server %s:%d",
				cfg_read_key_df("distro_connect_ip", DISTRO_CONNECT_IP), int_cfg_read_key_df("distro_connect_port", DISTRO_CONNECT_PORT));

		if ((m = sock_connect(cfg_read_key_df("distro_connect_ip", DISTRO_CONNECT_IP), int_cfg_read_key_df("distro_connect_port", DISTRO_CONNECT_PORT)))) {

			loge(LOG_INFO,
				"Connection established to master server %s:%d",
					cfg_read_key_df("distro_connect_ip", DISTRO_CONNECT_IP), int_cfg_read_key_df("distro_connect_port", DISTRO_CONNECT_PORT));

			count = read(m->fd, buffer, 1024);

			if (count != 1024) {
				loge(LOG_, "Expected ``primer'' from host, but did not receive it.  Waiting 60 seconds and will retry.");
				sock_close(m);
				mypause_time(1000 * 60);
				continue;
			}

			if (Ctrl_UnPrime(buffer) != 1) {
				loge(LOG_, "The primer received from the master server is a mismatch; it was not expected.  Waiting 60 seconds and will retry.");
				sock_close(m);
				mypause_time(1000 * 60);
				continue;
			}
			
			sock_nonblock(m);

			execone = 0;
			for (;;) {
				char	lbuf[1048576];
				int	mys, appx, delta;

				/* attempt to read the data directly from the socket */
				if ((count = read(m->fd, buffer, sizeof buffer - 1)) > 0) {
					/* successful?  add it to the buffer! */
					MemLock();
					input_buffer = bytes_append(input_buffer, buffer, count);
					MemUnlock();
				} else if(count < 0) {
					/* The socket was "just busy" -- we don't do anything here */
					if(errno == EAGAIN || errno == EWOULDBLOCK) {
						; /* do nothing at all :) */
					} else {
						/* ut oh, it is dead, wtf */
						/* close the socket */
						sock_close(m);

						/* kill the buffer */
						bytes_free(input_buffer);

						/* Set the input_buffer to NULL */
						input_buffer = NULL;

						/* now leave, we should automatically try to reconnect */
						break;
					}			
				}

				if ((bytes_size(input_buffer) > min_buffer) && !execone) {
					ms_diff_init(&diff);
					ms_diff_set_difference(&diff, 100);
					execone = !0;
					continue;
				} else {
					mypause_fd(m->fd, 100);
				}

				/*
				 * If there's nothing in our buffer, then we should wait and try again
				 * very soon.
				 */
				if (bytes_size(input_buffer) <= 0) {
					mypause_fd(m->fd, 500);
					continue;
				}

				if (!(delta = ms_diff(&diff))) {
					mypause_fd(m->fd, 100);
					continue;
				}

				/* Get the size to copy */
				appx = trans_quot(delta, 1, 22050, 2);

				/* Do not overflow the buffer :) */
				if (appx > sizeof lbuf)
					appx = sizeof lbuf;

				/* pull the data in from the buffer that we have up to 'appx' amount */
				if ((mys = bytes_extract(input_buffer, lbuf, appx)) <= 0)
					continue;

				MemLock();
				for (scan = AllThreadsHead; scan; scan = scan->next) 
					if (IsStatusStream(scan) && !IsAdvertising(scan))
						if (bytes_size(scan->rbuf) < CLIENT_MAX_BUFSIZE)
							scan->rbuf = bytes_append(scan->rbuf, lbuf, mys);

				MemUnlock();
			}
		}

		/* make a log entry about this */
		loge(LOG_ERR, "Unable to connect to master server %s:%d.  Waiting 10 seconds and re-trying",
			cfg_read_key_df("distro_connect_ip", DISTRO_CONNECT_IP), int_cfg_read_key_df("distro_connect_port", DISTRO_CONNECT_PORT));

		/* wait 10 seconds, then we should kick off a re-connect */
		mypause_time(1000 * 10);

	}

}

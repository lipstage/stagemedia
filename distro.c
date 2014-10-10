#include "stagemedia.h"

int	distro_init(void) {
	int	pid, serverfd;
	pthread_t	cleanup, frommaster;

	if (fork())
		exit(0);

	/* Ignore the PIPE signal */
	signal(SIGPIPE, SIG_IGN);

	/* Attempt to bind to the address we'll use for HTTP/streaming */
	if ((serverfd = bind_address("0.0.0.0", 8055)) < 0) {
		fprintf(stderr, "mode=dist; unable to bind to address or socket\n");
		exit(-1);
	}

	for (;;) {
		if ((pid = fork()) == 0) {
			/* start pulling data from the master server */
			pthread_create(&frommaster, NULL, GetFromMaster, NULL);

			/* this is the cleanup job which removes old, stale, or dead connections */
			pthread_create(&cleanup, NULL, CleanUp, NULL);

			/* give back the serverfd! */
			return (serverfd);
		} else if (pid > 0) {
			waitpid(pid, NULL, 0);	
		}
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
	/* time_t	st_time = time(0); */
	char	sessionid[1024];

	/* set the connect time, NOW */
	s->connect_epoch = time(0);

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

	if (http_fetch_request(s->sock, &method, sessionid))
		task_finish(s);		/* exit */

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

		for (scan = AllThreadsHead; scan; scan = scan->next) {
			char	status_name[512];
			time_t	seconds_connected = 0;

			/* get the status of the connected process/thread/socket - ish */
			task_string_status(scan, status_name, sizeof status_name);

			/* number of seconds connected */
			seconds_connected = ( time(0) - scan->connect_epoch );

			/* this is the format we're going to push out */
			snprintf(buffer, sizeof buffer - 1, "%d: ip_addr=%s,status=%s,con_sec=%lu,pcm_buf=%d\r\n", 
				++count, 
				scan->sock ? scan->sock->ip_addr_string : "N/A",
				status_name,
				seconds_connected,
				bytes_size(scan->rbuf)
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
		if (*sessionid == 0 || (valid_session(sessionid) <= 0)) {
			http_send_header(s->sock, 403, "text/plain");
			task_finish(s);
		}
	}

	/* Send the HTTP header */
	if (http_send_header(s->sock, 200, "audio/mpeg"))
		task_finish(s);		/* exit */

	/* We will now accept the PCM data */
	SetStatus_Stream(s);
	
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

		if (!bytes_ready(s->rbuf)) {
			/* Unlock the memory area -- allow someone else to use it */
			MemUnlock();

			/* wait up to 50ms */
			mypause_time(50);

			/* restart */
			continue;
		}

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
			fprintf(stderr, "mcopy modulus is off!\n");
		}

		/* grab the data */
		bytes_extract(s->rbuf, audio, mcopy);

		/* Unlock the memory now */
		MemUnlock();
			
		/* Looks like this bus is done */
		if ((encode_mp3_data(encode, audio, mcopy / 2, SocketPush, s)) < 0) {
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
		if ((m = sock_connect("localhost", 18101))) {
			count = read(m->fd, buffer, 1024);

			if (count != 1024) {
				fprintf(stderr, "Expected the primer, didn't get it\n");
				exit(-1);
			}

			if (Ctrl_UnPrime(buffer) != 1) {
				fprintf(stderr, "Primer version mismatch\n");
				exit(-1);
			}
			
			sock_nonblock(m);

			execone = 0;
			for (;;) {
				char	lbuf[1048576];
				int	mys, appx, delta;

				/* attempt to read the data directly from the socket */
				if ((count = read(m->fd, buffer, sizeof buffer - 1)) > 0) {
					/* successful?  add it to the buffer! */
					//printf("inside!!!! %d\n", count); fflush(stdout);
					MemLock();
					input_buffer = bytes_append(input_buffer, buffer, count);
					MemUnlock();
					//printf("after!!!!\n"); fflush(stdout);
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

				//printf("POSITION 2\n");fflush(stdout);

				/*
				 * If there's nothing in our buffer, then we should wait and try again
				 * very soon.
				 */
				if (bytes_size(input_buffer) <= 0) {
					mypause_fd(m->fd, 500);
					continue;
				}

				//printf("POSITION 3\n");fflush(stdout);
			
				if (!(delta = ms_diff(&diff))) {
					mypause_fd(m->fd, 100);
					continue;
				}

				//printf("POSITION 4\n");fflush(stdout);
				/* Get the size to copy */
				appx = trans_quot(delta, 1, 22050, 2);

				//printf("POSITION 5\n");fflush(stdout);

				/* Do not overflow the buffer :) */
				if (appx > sizeof lbuf)
					appx = sizeof lbuf;

				//printf("POSITION 6\n");fflush(stdout);

				/* pull the data in from the buffer that we have up to 'appx' amount */
				if ((mys = bytes_extract(input_buffer, lbuf, appx)) <= 0)
					continue;

				//printf("POSITION 7\n");fflush(stdout);

				MemLock();
				for (scan = AllThreadsHead; scan; scan = scan->next) 
					if (IsStatusStream(scan) && !IsAdvertising(scan))
						if (bytes_size(scan->rbuf) < CLIENT_MAX_BUFSIZE)
							scan->rbuf = bytes_append(scan->rbuf, lbuf, mys);

				MemUnlock();
			}
		}

		//puts("Waiting to reconnect");

		/* wait 10 seconds, then we should kick off a re-connect */
		mypause_time(1000 * 10);

		//puts("Reconnecting");
	}

}

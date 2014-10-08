#include "slingmedia.h"

int	SocketPush();

/*
 * If master is set to on
 */
int	MASTER = 0;

void	*ProcessHandler();
void	*DistHandler();
void	*CleanUp();
void	*GetFromMaster();

int	main(void) {
	int	serverfd;
	pthread_t	cleanup, master;
	pSocket	s;
	int	masterfd;
	pthread_t	frommaster;

	/* load the configuration file settings */
	read_config();

	/* refuse to accept SIGPIPE, it kills */
	signal(SIGPIPE, SIG_IGN);

	/* Is master turned on? */
	if (cfg_read_key("master") && !strcasecmp(cfg_read_key("master"), "true"))
		MASTER = 1;

	/*
	 * If we are running in master mode
	 */
	if (MASTER) {
		/* kick off the master server thread :) */
		pthread_create(&master, NULL, MasterServer, NULL);
		masterfd = bind_address("0.0.0.0", 18101);
		if (masterfd < 0) {
			fprintf(stderr, "mode=master; Could not bind to master addresses and/or ports [for dist servers]\n");
			return -1;
		}
		/* for ease of use :) */
		serverfd = masterfd;
	} else {
		if ((serverfd = bind_address("0.0.0.0", 8055)) < 0) {
			fprintf(stderr, "mode=dist; unable to bind to address or socket\n");
			return -1;
		}

		pthread_create(&frommaster, NULL, GetFromMaster, NULL);
	}

	/* This is the back-end 'cleanup' job */
	pthread_create(&cleanup, NULL, CleanUp, NULL);

	/* while (1) equiv */
	for (;;) {
		pThreads	ns;

		s = sock_accept(serverfd);

		if (s) {
			int	ret, connections = 0;
			pThreads	scan;

			MemLock();

			/*
			 * Get total connection count
			 */
			for (scan = AllThreadsHead; scan ; scan = scan->next) 
				if (IsStatusStream(scan))
					++connections;
			MemUnlock();

			ns = NewSockets();

			if (!ns) {
				/* probably should say something here, too */
				sock_close(s);
				MemUnlock();
				continue;
			}

			/* set it to the socket */
                        ns->sock = s;

			if (!MASTER)
				ret = pthread_create(&ns->handler, NULL, ProcessHandler, (void *)ns);
			else {
				/*
				 * We cannot go over the maximum allowed connections
				 */
				int	n = atoi(cfg_read_key_df("distro_max_clients", "0"));

				if (n <= 0)
					n = MASTER_MAX_CLIENTS;

				/* close it up */
				if (connections >= n) {
					sock_close(s);
					SetStatus_Dead(ns);
					continue;
				}

				ret = pthread_create(&ns->handler, NULL, DistHandler, (void *)ns);
			}

			/* Could be out of threads or something else */
			if (ret) {
				/* probably should say something here */
				sock_close(s);
		
				/* For later */
				// FreeSockets(ns)
			}
		}
	}
	

	printf("%d\n", serverfd);
	
	return 0;
}

void	*CleanUp() {
	struct	timeval	tv;

	for (;;) {
		pThreads	scan;

		tv.tv_sec = 15;
		tv.tv_usec = 0;
		select(1, NULL, NULL, NULL, &tv);	

		MemLock();

		for (scan = AllThreadsHead; scan; scan = scan->next) {
			if (IsStatusWaitRQ(scan)) {
				if ( (time(0) - scan->last) > 15) {
					close (scan->sock->fd);
					SetStatus_Dead(scan);
				}
			}
		}
	
		MemUnlock();		
	}
}

void	*DistHandler(pThreads s) {
	int	n = 0;
	char	primer[1024];

	/* This is where we'll put our primer! */
	memset(primer, 0, sizeof primer);

	/* Set up the primer */
	Ctrl_Primer(1, primer, sizeof primer);

	/* Write it out -- now */
	write(s->sock->fd, primer, sizeof primer);

	/* We are ready IMMEDIATELY! */
	SetStatus_Stream(s);

	/* Always non-blocking */
	sock_nonblock(s->sock);

	for (;;) {
		MemLock();
 		if (s->rdb.si <= 0) {
			MemUnlock();
			mypause();
			continue;
		} else {
			int	count;

			// This should be addressed, lol, bad code, really bad
			count = write(s->sock->fd, s->rdb.buf, s->rdb.si);

			/*
			 * Data was written to the client
			 */
			if (count > 0) {
				if (count == s->rdb.si) {
					free (s->rdb.buf);
					s->rdb.si = 0;
					s->rdb.buf = NULL;
				} else {
					void *temp;
				
					memmove(s->rdb.buf, &((unsigned char *)s->rdb.buf)[count], s->rdb.si - count);
					if (!(temp = realloc(s->rdb.buf, s->rdb.si - count))) {
						// need to handle this better
					}
					if (temp != s->rdb.buf)
						s->rdb.buf = temp;
					s->rdb.si -= count;
				}
			/*
			 * No data was written, perhaps some buffers are full
			 */
			} else if (count == 0) {
				MemUnlock();
				mypause();
				continue;
			/*
			 * Potential error happened here
			 */
			} else {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					MemUnlock();
					mypause();
					continue;
				} else {
					SetStatus_Dead(s);
					if (s->rdb.buf)
						free (s->rdb.buf);
					sock_close(s->sock);
					MemUnlock();
					break;
				}
			}

		}

		/*
		 * check out maximum and die in case 
		 */
		if (s->rdb.si >= DISTRO_MAX_SIZE) {
			SetStatus_Dead(s);
			if (s->rdb.buf)
				free (s->rdb.buf);
			sock_close(s->sock);
			MemUnlock();
			break;
		}
		MemUnlock();
	}
	pthread_exit(&n);
	return NULL;
}

/*
 * Each connection becomes a 'thread' in which the user/connection executes.
 * The 'ProcessHandler' function is actually one of the thread managers (mini-process)
 */
void	*ProcessHandler(pThreads s) {
	//int	ret = 0;
	EncMP3	encode;
	AUFormat	au;

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

	//puts("*** STARTING CONNECTION");

	sock_block(s->sock);

	if (http_fetch_request(s->sock)) {
		int	n = -1;
		
		sock_close(s->sock);
		SetStatus_Dead(s);
		pthread_exit(&n);
	}
	//puts("*** START STREAMING");
	//printf("%d\n", ret);

	/* Send the HTTP header */
	if (http_send_header(s->sock, 200, "audio/mpeg")) {
		int	n = -1;
		sock_close(s->sock);
		SetStatus_Dead(s);
		pthread_exit(&n);
		/* SHOULD DO MORE HERE */
	}

	/* The socket will now be non-blocking */
	//sock_nonblock(s->sock);

	/* We will now accept the PCM data */
	SetStatus_Stream(s);
	
	/* Get the encoder ready son */
	encode = encode_mp3_init(au);

	for (;;) {
		/* Wait up to 10ms */
		//mypause_fd(s->sock->fd, 10);
		//mypause();

		/* Lock the memory for thread-safe */
		MemLock();

		if (s->rdb.si) {
		//if (bytes_size(s->rbuf)) {
			short	int	*audio;
			int	mcopy;
			//int	maxamount = (s->rdb.si < 1024 ? s->rdb.si :  1024);
			int	maxamount = s->rdb.si;

			if (!(audio = calloc(2, maxamount / 2)))
				{ /* handle it */ }
		
			/* calculate the squeeze */
			mcopy = (maxamount / 2) * 2;

			/* copy and squeeze out any non-16 bitness :) */
			memcpy(audio, s->rdb.buf, mcopy);
	
			if (mcopy == s->rdb.si) {
				free (s->rdb.buf);
				s->rdb.buf = NULL;
				s->rdb.si = 0;
			} else {
				char	*temp;

				memmove(s->rdb.buf, &((unsigned char *)s->rdb.buf)[mcopy], s->rdb.si - mcopy);
				temp = realloc(s->rdb.buf, s->rdb.si - mcopy);
				if (temp != NULL)
					s->rdb.buf = temp;
				s->rdb.si -= mcopy;
			}

			MemUnlock();
			
			encode_mp3_data(encode, audio, mcopy / 2, SocketPush, s);

			free (audio);
			
			
		} else {
			/* Unlock the memory */
			MemUnlock();
		
			mypause();
		}
	}


	/* IF THE STATUS IS DEAD... */
	/* We ended, we have modifications to make too */
	MemLock();

	printf("It died!\n");

	/* And done! */
	MemUnlock();

	return NULL;
}

int	SocketPush(pThreads s, unsigned char *data, int size) {
	sock_block(s->sock);
	write(s->sock->fd, data, size);
	return 0;
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
				count = read(m->fd, buffer, sizeof buffer);

				//if (count > 0) {
				//	input_buffer = bytes_append(input_buffer, buffer, count);
				//}

				if (count > 0) {
				//if (execone || bytes_size(input_buffer) > 128000) {
					char	lbuf[16384];
					int	mys;

					execone = 1;
					//mys = bytes_peek(input_buffer, lbuf, sizeof lbuf);
					//bytes_squash(input_buffer, mys);

					MemLock();
					for (scan = AllThreadsHead; scan; scan = scan->next) {
						if (IsStatusStream(scan)) {
							add_to_rdb(scan, buffer, count);
							//printf("push->%lu, input_buffer->%u\n", scan->rdb.si, bytes_size(input_buffer));
							//add_to_rdb(scan, lbuf, mys);
							//scan->rbuf = bytes_append(scan->rbuf, buffer, count);
						}	
					}
					MemUnlock();
				} else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
					mypause_fd(m->fd, 10);
				} else {
				}

			}
		}
		sleep(10);
	}

}

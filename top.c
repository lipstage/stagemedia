#include "stagemedia.h"

int	SocketPush();

/*
 * If master is set to on
 */
int	MASTER = 0;

void	*ProcessHandler();
void	*DistHandler();
void	*CleanUp();
void	*GetFromMaster();

int	main(int argc, char **argv) {
	int	serverfd;
	pthread_t	cleanup, master;
	pSocket	s;
	int	masterfd, options;
	char	*config_file = CONF_FILE;

	/*
	 * Parse anything for getopt() first
	 */
	while ((options = getopt(argc, argv, "hc:")) != -1) {
		switch (options) {
			case 'c':
				config_file = optarg;
				break;
			case 'h':
				fprintf(stdout,
                        		"%s - StageMedia Server (%s)\n"
                        		"\n"
                        		"\t%-20sLoad specified file as config\n"
                        		"\t%-20sThis screen\n"
                        		"\n",
                        		*argv, STAGEMEDIA_VERSION,
					"-c <file>", "-h");
		                return -1;
				break; /* UNREACHABLE */
			default:
				abort ();	
		}
	}

	/* load the configuration file settings */
	read_config(config_file);

	if (fork())
		exit(0);

	/* refuse to accept SIGPIPE, it kills */
	signal(SIGPIPE, SIG_IGN);

	/* Is master turned on? */
	if (cfg_is_true("master", 0))
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
		pthread_create(&cleanup, NULL, CleanUp, NULL);
	} else {
/*
		if ((serverfd = bind_address("0.0.0.0", 8055)) < 0) {
			fprintf(stderr, "mode=dist; unable to bind to address or socket\n");
			return -1;
		}

		pthread_create(&frommaster, NULL, GetFromMaster, NULL);
*/
		serverfd = distro_init();
	}

	/* This is the back-end 'cleanup' job */
	//pthread_create(&cleanup, NULL, CleanUp, NULL);

	/* wait about 3 seconds */
	mypause_time(3);

	/* go into the background */
	//if (fork())
	//	exit(0); 

	/* while (1) equiv */
	for (;;) {
		pThreads	nt;

		s = sock_accept(serverfd);

		if (s) {
			int	ret, connections = 0;

			/*
			 * Get a total count of the connections
			 */
			connections = task_count();

			MemLock();

			/*
			 * Attempt to create a new task -- if it fails, run this.
			 */
			if (!(nt = new_task())) {
				/* close the socket -- not much more we can do :( */
				sock_close(s);

				/* Unlock the memory */
				MemUnlock();

				/* Restart our loop again */
				continue;
			}

			MemUnlock();

			/* set it to the socket */
                        nt->sock = s;

			if (!MASTER)
				ret = pthread_create(&nt->handler, NULL, ProcessHandler, (void *)nt);
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
					SetStatus_Dead(nt);
					continue;
				}

				ret = pthread_create(&nt->handler, NULL, DistHandler, (void *)nt);
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


void	*DistHandler(pThreads s) {
	int	n = 0, count = 0;
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

		/*
		 * Test if we have something in our buffer.  If we don't, 
		 * then we wait about 250ms and fight another day.
		 */
		if (bytes_size(s->rbuf) <= 0) {
			/* Don't keep the lock */
			MemUnlock();

			/* Wait 250ms and expect additional data */
			mypause_time(250);

			/* Restart inside the loop */
			continue;
		}

		/* write as much as we possibly can */
		count = write(s->sock->fd, bytes_data(s->rbuf), bytes_size(s->rbuf));

		/*
		 * Go ahead and 'shrink' the data area of what we've already written
		 */
		if (count > 0) {
			bytes_squash(s->rbuf, count);

		/*
		 * Could just not be connected, OR it may be "busy"
		 */
		} else if (count < 0) {
			
			/*
			 * The socket isn't dead -- it is just quite busy right now.
			 * We will wait up to half a second, allowing for a recovery period.
			 * After that time, we'll try everything again.
			 *
			 * We also REFUSE to allow the buffer to go over the DISTRO_MAX_SIZE in config.h, 
			 * which is probably set for 4 megabytes.  This is "likely" around 95 seconds of audio
			 * at PCM 16-bit, mono, 22050 sample rate.
			 */
			if ((errno == EAGAIN || errno == EWOULDBLOCK) && bytes_size(s->rbuf) < DISTRO_MAX_SIZE) {
				/* Remove the lock */
				MemUnlock();
		
				/* Wait up to 500ms for the socket to become "ready" */
				mypause_fd(s->sock->fd, 500);

				/* restart the loop */
				continue;
			} else {
				/*
				 * The socket has died in this case :(
				 */

				/* Kill the buffer */
				bytes_free(s->rbuf);

				/* close the socket */
				sock_close(s->sock);

				/* Set the socket as 'dead' */
				SetStatus_Dead(s);
			
				/* Unlock memory */
				MemUnlock();

				/* ...And, we're done */
				break;
			}
			
		}

		/* Unlock the memory stuff */
		MemUnlock();
	}
	pthread_exit(&n);
	return NULL;
}


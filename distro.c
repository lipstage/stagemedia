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
		MEMLOCK(1);

		/* Loop through all tasks */
		for (scan = AllThreadsHead, nopause = 0; scan; scan = scan->next) {
			/*
			 * The task/job is complete, we just need to finish it out
			 */
			if (IsStatusFinish(scan)) {
				/* free the memory! */
				MEMUNLOCK(1);

				/* Join until exit -- should not take very long at all */
				debug3("%s: Joining a thread, hope to see you on the other side.", __FUNCTION__);
				(void) pthread_join(scan->handler, NULL);
				debug3("%s: We have exited the thread.", __FUNCTION__);

				MEMLOCK(2);

				/* Mark it as dead */
				SetStatus_Dead(scan);
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
	
		MEMUNLOCK(1);
	}
}


/*
 * Each connection becomes a 'thread' in which the user/connection executes.
 * The 'ProcessHandler' function is actually one of the thread managers (mini-process)
 */
void	*ProcessHandler(pThreads s) {
	AUFormat	au;
	int	method = 0;
	//int	max_stream_time = cfg_read_key("max_stream_time") ? atoi(cfg_read_key("max_stream_time")) : MAX_STREAM_TIME;
	const char	*quality = cfg_read_key_df("quality", DEFAULT_QUALITY);
	/* time_t	st_time = time(0); */
	char	sessionid[1024];
	pThreads	encoder;
	char	identity[64];

	/* set the connect time, NOW */
	s->connect_epoch = time(0);

	/* zero out the sessionid */
	memset(s->sessionid, 0, sizeof s->sessionid);

	/* debug entry to track when it started */
	loge(LOG_DEBUG, "(fd: %d, %s:%s) Client connected and distribution thread kicked off at %lu epoch",
		s->sock->fd, s->sock->ip_addr_string, s->sock->port_string, s->connect_epoch);

	if (!strcasecmp(quality, "good")) {
		au.channels = 1;
		au.data_offset = -1;
		au.data_size = -1;
		au.encoding = 3;
		au.sample_rate = 44100;
	} else if(!strcasecmp(quality, "verygood")) {
		au.channels = 2;
		au.data_offset = -1;
		au.data_size = -1;
		au.encoding = 3;
		au.sample_rate = 44100;
	} else {
		au.channels = 1;
	        au.data_offset = -1;
		au.data_size = -1;
		au.encoding = 3;
		au.sample_rate = 22050;
	}

	/* Tell other processes that we're starting off */
	SetStatus_Begin(s);

	/* The time of 'last' big change */
	s->last = time(0);

	/* Waiting for request completion */
	SetStatus_WaitRQ(s);

	sock_block(s->sock);

	/* Get data about the http request */
	if (http_fetch_request(s->sock, &method, sessionid, &s->request)) {
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
		int		sz;

		/* Send the plain/text header, and, if not, close the socket and stuff */
		if (http_send_header(s->sock, 200, "text/plain", NULL, NULL))
			task_finish(s);

		/* Let it know we are gathering stats */
		SetStatus_Stats(s);

		/* Lock the memory to prevent changes */
		MEMLOCK(1);

		snprintf(buffer, sizeof buffer - 1, "V %s\r\n", STAGEMEDIA_VERSION);
		bytes_append(s->wbuf, buffer, strlen(buffer));
 
		for (scan = AllThreadsHead; scan; scan = scan->next) {
			char	status_name[512];
			time_t	seconds_connected = 0;

			/* get the status of the connected process/thread/socket - ish */
			task_string_status(scan, status_name, sizeof status_name);

			/* number of seconds connected */
			seconds_connected = ( time(0) - scan->connect_epoch );

			/* this is the format we're going to push out */
			snprintf(buffer, sizeof buffer - 1, "C %d: ip_addr=%s,status=%s,con_sec=%lu,rbuf=%d,wbuf=%d,sessionid=%s,identity=%s\r\n", 
				++count, 
				scan->sock ? scan->sock->ip_addr_string : "N/A",
				status_name,
				seconds_connected,
				bytes_size(scan->rbuf),
				bytes_size(scan->wbuf),
				!*scan->sessionid ? "" : scan->sessionid,
				*scan->identity ? scan->identity : "N/A"
			);
			bytes_append(s->wbuf, buffer, strlen(buffer));
		}

		/* Unlock the memory now, ok :) */
		MEMUNLOCK(1);

		while ((sz = bytes_extract(s->wbuf, buffer, sizeof buffer)) > 0) 
			SocketPush(s, buffer, sz);

		/* close the socket now */
		task_finish(s);
	}

	/* A bit of a hack used for our streaming */
	if (!s->request.use_range && strstr(s->request.user_agent, "Lipstage/1")) {
		s->request.use_range = 1;
		s->request.range.min = 0;
		s->request.range.max = -1;
	}

	/* Generate an Etag */
	random_string(identity, sizeof identity - 1);
	strncpy(s->etag, identity,
		sizeof s->etag < sizeof identity ? sizeof s->etag : sizeof identity);

	/* copy over my session information */
	if (*sessionid)
		strncpy(s->sessionid, sessionid, sizeof(s->sessionid) - 1);

	/* In the event we've already established a stream, we should avoid the session check nastiness */
	if (*s->request.lipstage_session) {
		if ((task_by_identity(s->request.lipstage_session, 0))) {
			serve_from_identity(s, s->request.lipstage_session);
			http_send_header(s->sock, 500, "text/plain", NULL, NULL);
			task_finish(s);
		}
	}

	/* We have too many connections */
	if (cfg_read_key("max_clients") && atoi(cfg_read_key("max_clients")) <= task_count()) {
		if (http_send_header(s->sock, 500, NULL, NULL, NULL)) 
			task_finish(s);
	}

	/* user must present a valid cookie -- with login */
	if (cfg_is_true("api_session_check", 1)) {

		/* Invalid sesion.  The API says "no" to this */
		if (*sessionid == 0 || (valid_session(sessionid) <= 0)) {
			http_send_header(s->sock, 403, "text/plain", NULL, NULL);
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


	/*
	 * First, we kick off an encoder task.
	 */
	encoder = new_task();

	/* It isn't attached to any socket! */
	encoder->sock = NULL;

	/* encoder takes on this connection's identity */
	strncpy(encoder->identity, identity, sizeof encoder->identity);

	/* We also copy the session information, provided it is available */
	if (*s->sessionid != '\0')
		strncpy(encoder->sessionid, s->sessionid, sizeof encoder->sessionid - 1);

	/* copy over the AU audio format so the encoder will know how to handle it */
	memcpy(&encoder->au, &au, sizeof encoder->au);

	/* Kick it off */
	pthread_create(&encoder->handler, NULL, EncodeHandler, (void *)encoder);

	/* Pause for 100ms -- allow the encoder to do something :) */
	mypause_time(100);

	/* Service the request */
	serve_from_identity(s, identity);
	
	return NULL;		/* UNREACHABLE */
}


void	*EncodeHandler(void *arg1) {
	EncMP3	encode;
	pThreads	s = arg1;
	AUFormat	au;
	short	int	audio[MAX_TRANSCODE_CLIP_SIZE + sizeof(short int)] = { 0 };
	int	sz = 0;
#ifdef INIT_BURST_ON_CONNECT
	int	min_buffer;
	unsigned long long	bytes_encoded = 0;
#endif
	memcpy(&au, &s->au, sizeof au);
	pBytes		earea;

	/* set the connect time, NOW */
	s->connect_epoch = time(0);

	/* Tell other processes that we're starting off */
	SetStatus_Virtual(s);

	/* The time of 'last' big change */
	s->last = time(0);

#ifdef	INIT_BURST_ON_CONNECT
	min_buffer = trans_buf_min(1, au.channels, au.sample_rate, 2);
#endif
	/* Get the encoder ready son */
	encode = encode_mp3_init(au);

	earea = bytes_init();
	bytes_type_set(earea, BYTES_TYPE_RING);
	bytes_maxsize_set(earea, BYTES_MAXSIZE_PER_USER);

	for (;;) {
		int		mcopy;
		int		maxamount;

		/* Lock the memory for thread-safe */
		MEMLOCK(1);

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
		 * until there are bytes
		 */
		
#ifdef INIT_BURST_ON_CONNECT
		if (!bytes_ready(s->rbuf) || bytes_size(s->rbuf) < min_buffer) {
#else
		if (!bytes_ready(s->rbuf)) {
#endif
			/* Unlock the memory area -- allow someone else to use it */
			MEMUNLOCK(1);

			/* wait up to 100ms */
			mypause_time(100);

			/* restart */
			continue;
		}

		maxamount = bytes_size(s->rbuf);
		if (maxamount > MAX_TRANSCODE_CLIP_SIZE)
			maxamount = MAX_TRANSCODE_CLIP_SIZE;

		memset(audio, 0, sizeof audio);

		if (au.channels == 1) {
			/* calculate the squeeze */
			mcopy = (maxamount / sizeof(short int)) * sizeof(short int);

			if ( (mcopy % 2) != 0) {
				loge(LOG_WARN, "EncodeHandler: modulus operator expected 0, but is not.");
			}
		} else if (au.channels == 2) {
			mcopy = (maxamount / (sizeof(short int) * 2)) * (sizeof(short int) * 2);
			if ( (mcopy % 4) != 0) {
				loge(LOG_WARN, "EncodeHandler: modulus operator expected 0, but is not.");
			}
		}

		/* grab the data and actual size */
		sz = bytes_extract(s->rbuf, audio, mcopy);
#if defined(INIT_BURST_FLASHBACK) && defined(INIT_BURST_ON_CONNECT)
		task_lock(s);
		if (sz != mcopy)
			debug("sz differs from mcopy (potential bug here): sz=%d mcopy=%d", sz, mcopy);
		s->init = 1;
		task_unlock(s);
#endif

		/* keep record of total bytes encoded */
		bytes_encoded += sz;

		/* Unlock the memory now */
		MEMUNLOCK(1);

		/* Looks like this bus is done */
		if ((encode_mp3_data(encode, audio, mcopy / 2, PH_BytesSocketPush, earea)) < 0) {
			/* We are going to close the encoder, gracefully */
			encode_mp3_close(encode);

			/* Write out to a log what just happened */
			loge(LOG_DEBUG2, "EncoderHandler: encode_mp3_data returned less than 0, closing out background encoding job");

			bytes_free(earea);

			/* we now exit */
			task_finish(s);
		}

		task_lock(s);

		/*
		 * Did we exceed time?
		 */
		if ( time(0) - s->last > MAX_ASYNC_ENCODE_TIME) {
        		debug("%s: Exceeding time limit, discontinuing the background encoder due to timeout.", __FUNCTION__);

			encode_mp3_close(encode);
			bytes_free(earea);
			task_unlock(s);
			task_finish(s);
		}

		/*
		 * Test to see if we should end or not 
		 */
		if (s->request_end) {
			debug2("%s: Requested that we end by a raising of ``request_end''", __FUNCTION__);
			encode_mp3_close(encode);

			bytes_free(earea);

			task_unlock(s);
			task_finish(s);
		}

		/*
		 * Move earea to our write area 
		 */
		if (bytes_size(earea) > 0) {
			MEMLOCK(1);
			bytes_copy(s->wbuf, earea, -1);
			MEMUNLOCK(1);
			bytes_purge(earea);
		}	
		task_unlock(s);
	}

	/* we're done! */
	task_finish(s);
	
	return NULL;		/* UNREACHABLE */
}

int     SocketPush(pThreads s, unsigned char *data, int size) {
	sock_block(s->sock);
	return write(s->sock->fd, data, size);
}

int	PH_BytesSocketPush(pBytes bbuf, unsigned char *data, int size) {
	bytes_append(bbuf, data, size);
	return 0;
}

int	serve_from_identity(pThreads myself, const char *identity) {
	pThreads	source = NULL;
	int		ByteServing = 0, iter = 0, ret;

	/*
	 * Log the details about the identity we are "aiming" to serve.
	 */
	loge(LOG_DEBUG,
		"serve_from_identity: Serving up identity details for %s on %s:%s on fd:%d",
			identity, myself->sock->ip_addr_string, myself->sock->port_string, myself->sock->fd);

	if (myself->request.use_range) {
		debug("%s: Using range (byte serving) to serve content", __FUNCTION__);
		ByteServing++;
	}

	for (;;) {
		/* fetch the source, and also lock it */
		source = task_by_identity(identity, !0);


		// Well, we probably need a way to "lock" the dang individual threads so we
		// don't slam it twice. Making a note of that here.
		if (source) {
			pHTTPHeader header;
			int	sz = 0, sent;
			char	temp[MAX_TRANSCODE_CLIP_SIZE > 32768 ? MAX_TRANSCODE_CLIP_SIZE / 2 : 32768];

			/* Try to get bytes off the write buffer. If we can't, we just wait 100ms and unlock the resource */
#if defined(INIT_BURST_FLASHBACK) && defined(INIT_BURST_ON_CONNECT)
			if (!source->init || (sz = bytes_peek(source->wbuf, temp, sizeof temp)) < 0) {			
#else
			if ((sz = bytes_peek(source->wbuf, temp, sizeof temp)) < 0) {
#endif
				task_unlock(source);
				mypause_time(250);
				continue;
			}
		
			/* Get the actual number of sent bytes */
			sent = source->stats.wbuf_sent;

			/* Remove the lock on the resource */
			task_unlock(source);

			if (ByteServing) {
				/* build the HTTP headers we send to the client */
				header = http_header_init();
				http_header(header, "X-Lipstage-Session", "%s", identity);
				http_header(header, "Accept-Ranges", "bytes");
				http_header(header, "Content-Range", "bytes %d-%d/2147483648", sent, sent + sz);
				http_header(header, "X-Lipstage-SendBytes", "%d", sz);
			
				debug2("%s: Byte Serving debugging details; requested(min=%d, max=%d), sending(start=%d, finish=%d) [%s]",
					__FUNCTION__, myself->request.range.min, myself->request.range.max,
					sent, sent + sz,
					(sent != myself->request.range.min ? "differs" : "same"));

				/* send the header -- identity is actually a const char * so a cast is fine here */
				if (http_send_header(myself->sock, 206, "audio/mpeg", (char *) identity, header)) {
					task_unlock(source);
					task_finish(myself);         /* exit */
				}	
			} else if (!iter) {
				http_send_header(myself->sock, 200, "audio/mpeg", NULL, NULL);
				iter++;
			}

			/* Attempt to push the bytes to the client */
			ret = PH_SocketPush(myself, temp, sz);
			if (ret > 0) {
				MEMLOCK(1);
				/* attempt to re-resolve the task */
				source = task_by_identity(identity, !0);
				if (source) {

					/* remove the bytes we used earlier */
					bytes_squash(source->wbuf, sz);

					/* Update the bytes sent */
					source->stats.wbuf_sent += sz;

					/* Update the last time we accessed it */
					source->last = time(0);

					/* Unlock the resource */
					task_unlock(source);
				}
				MEMUNLOCK(1);
			} else if (ret < 0) {
				if (!ByteServing) {
					source->request_end++;
					task_unlock(source);
					task_finish(myself);
				}
			}
			if (ByteServing)
				task_finish(myself);
		} else {
			if (!iter) {
				http_send_header(myself->sock, 404, NULL, NULL, NULL);
				task_finish(myself);
			} else {
				/* WTF */
				task_finish(myself);
			}
		}
	}

	return 0;
}

int	PH_SocketPush(pThreads s, unsigned char *data, int size) {
	int	nr;
	int	retries;

	for (retries = 0; retries < 30; retries++) {
		sock_nonblock(s->sock);
		nr = write(s->sock->fd, data, size);
		sock_block(s->sock);

		if (nr < 0 && !WouldBlock(nr)) {
			return -1;
		} else if (WouldBlock(nr)) {
			if (size > 0) {
				/* changed from 100ms to 500ms */
				mypause_write_fd(s->sock->fd, 500);
				continue;
			}
			return -1;
		} else if (nr > 0 && nr != size) {
			int	newsize = size - nr;

			/* log that we got a short count, just because, it is fun :) */
			loge(LOG_DEBUG2, "PH_SocketPush: Received a short count when writing. Wrote %d bytes, but should have been %d bytes, resending others",
				nr, size); 

			/* move the rest to the front */
			memmove(data, &data[nr], newsize);

			/* wait up to 1 second, for funzies (and better CPU management) */
			mypause_write_fd(s->sock->fd, 1000);

			/* set the size */
			size = newsize;

			/* and now just continue, it should work :) */
			continue;
		}
		return nr;
	}
	return -1;
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
	int		min_buffer;
			/* 5 second buffering time, 1 channel, 22050 samples/sec and 2 bytes (16-bit) per sample */
			/* Modified: quality will change this behavior!! */
	MSDiff		diff;
	CtrlPrimerHeader	primer;
	int		samplerate = 22050,		/* the "default" sampling rate */
			channels = 1;			/* the number of channels we have (may change later) */
#if defined(INIT_BURST_FLASHBACK) && defined(INIT_BURST_ON_CONNECT)
	pBytes	init_flashback = bytes_init();
#endif

#if defined(INIT_BURST_FLASHBACK) && defined(INIT_BURST_ON_CONNECT)
	if (init_flashback)
		bytes_type_set(init_flashback, BYTES_TYPE_RING);
#endif

	/* Set the minimum buffer -- to the default (could be over-ridden later) */
	min_buffer = trans_buf_min(8, 1, samplerate, 2);

	/* 0 the diff variable */
	memset(&diff, 0, sizeof diff);

	for (;;) {

		loge(LOG_INFO, 
			"Initiating connection to master server %s:%d",
				cfg_read_key_df("distro_connect_ip", DISTRO_CONNECT_IP), int_cfg_read_key_df("distro_connect_port", DISTRO_CONNECT_PORT));

		if ((m = sock_connect(cfg_read_key_df("distro_connect_ip", DISTRO_CONNECT_IP), int_cfg_read_key_df("distro_connect_port", DISTRO_CONNECT_PORT)))) {

#if defined(INIT_BURST_FLASHBACK) && defined(INIT_BURST_ON_CONNECT)
			MEMLOCK(1);

			if (init_flashback && bytes_size(init_flashback) > 0) {
				bytes_squash(init_flashback, bytes_size(init_flashback));
			}

			MEMUNLOCK(1);
#endif

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

			if (Ctrl_UnPrime(buffer, &primer) != 1) {
				loge(LOG_, "The primer received from the master server is a mismatch; it was not expected.  Waiting 60 seconds and will retry.");
				sock_close(m);
				mypause_time(1000 * 60);
				continue;
			}
			
			/* 
			** add the configuration for QUALITY to superhead
			** WARNING: If you change the quality on master, you HAVE to restart the distro server until
			** the functionality of remove from superhead is added
			*/
			if (primer.quality == QUALITY_GOOD) {
				/* one day: superhead_config_del("quality"); */
				superhead_config_add("quality", "good");

				/* need to update the minimum buffer and sample rate */
				samplerate = 44100;
				min_buffer = trans_buf_min(8, 1, samplerate, 2);

				debug("%s: Setting quality to \"good\" with min_buffer of %d", __FUNCTION__, min_buffer);
			} else if (primer.quality == QUALITY_VERYGOOD) {
				superhead_config_add("quality", "verygood");
	
				samplerate = 44100;
				channels = 2;
				min_buffer = trans_buf_min(8, 2, samplerate, 2);

				debug("%s: Setting quality to \"verygood\" with min_buffer of %d", __FUNCTION__, min_buffer);
			} else 
				debug("%s: Setting quality to \"acceptable\" with min_buffer of %d", __FUNCTION__, min_buffer);

#if defined(INIT_BURST_FLASHBACK) && defined(INIT_BURST_ON_CONNECT)
			if (init_flashback)
				bytes_maxsize_set(init_flashback, min_buffer);
#endif

			sock_nonblock(m);

			execone = 0;
			for (;;) {
				char	lbuf[1048576];
				int	mys, appx, delta;

				/* attempt to read the data directly from the socket */
				count = read(m->fd, buffer, sizeof buffer -1);


				/* conditionals */
				if (count > 0) {
					/* successful?  add it to the buffer! */
					MEMLOCK(2);

					input_buffer = bytes_append(input_buffer, buffer, count);

					MEMUNLOCK(2);

				} else if(count <= 0) {
					/* The socket was "just busy" -- we don't do anything here */
					if(count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
						; /* do nothing at all :) */
					} else {
						/* Log the event */
						loge(LOG_ERR, "Master server socket appears to be dead, issuing a stand-off/disconnect");

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
				/* } else { */
				} else if (!execone) {
					mypause_read_fd(m->fd, 100);	
					continue;
				}

				/*
				 * If there's nothing in our buffer, then we should wait and try again
				 * very soon.
				 */
				if (bytes_size(input_buffer) <= 0) {
					mypause_read_fd(m->fd, 100);	/* was 500 */
					continue;
				}

				if (!(delta = ms_diff(&diff))) {
					mypause_read_fd(m->fd, 100);
					continue;
				}

				/* Get the size to copy */
				appx = trans_quot(delta, channels, samplerate, 2);

				/* Do not overflow the buffer :) */
				if (appx > sizeof lbuf)
					appx = sizeof lbuf;

				/* pull the data in from the buffer that we have up to 'appx' amount */
				if ((mys = bytes_extract(input_buffer, lbuf, appx)) <= 0)
					continue;

#if defined(INIT_BURST_FLASHBACK) && defined(INIT_BURST_ON_CONNECT)
				if (init_flashback) {
					init_flashback = bytes_append(init_flashback, lbuf, mys);
				}
#endif

				MEMLOCK(3);

				for (scan = AllThreadsHead; scan; scan = scan->next) {
					if (IsStreamable(scan) && !IsAdvertising(scan)) {
#if defined(INIT_BURST_FLASHBACK) && defined(INIT_BURST_ON_CONNECT)
						if (!scan->flashback) {
							if (bytes_size(init_flashback) <= 0) {
								scan->flashback++;  /* screw it */
							} else {
								bytes_copy(scan->rbuf, init_flashback, -1);
								scan->flashback++;
								continue;
							}
						}
#endif
						if (bytes_size(scan->rbuf) < CLIENT_MAX_BUFSIZE) {
							scan->rbuf = bytes_append(scan->rbuf, lbuf, mys);
						}
					}
				}
				MEMUNLOCK(3);
			}
		}

		/* make a log entry about this */
		loge(LOG_ERR, "Unable to connect to master server %s:%d.  Waiting 10 seconds and re-trying",
			cfg_read_key_df("distro_connect_ip", DISTRO_CONNECT_IP), int_cfg_read_key_df("distro_connect_port", DISTRO_CONNECT_PORT));

		/* wait 10 seconds, then we should kick off a re-connect */
		mypause_time(1000 * 10);

	}

}

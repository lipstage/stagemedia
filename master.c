#include "stagemedia.h"

int	pcm_data_push(void *, int);

/*
 * MasterServer -
 *
 *  This is a thread/process which binds to port 18100 (PCM) and collects
 *  incoming data from the web/application front-end.  Once the data comes in,
 *  it will push it out via the streams
 */
void	*MasterServer()
{
	int	input;
	time_t	purge_timeout = 0;

	/* bind to the address and port */
	input = bind_address(
		cfg_read_key_df("master_bind_recv_ip", MASTER_BIND_RECV_IP), 
		int_cfg_read_key_df("master_bind_recv_port", MASTER_BIND_RECV_PORT));
	if (input < 0) {
		loge(LOG_CRITICAL, "Unable to bind to port %d (recording)", int_cfg_read_key_df("master_bind_recv_port", MASTER_BIND_RECV_PORT));
		exit(-1);
	}

	/*
	 * Loop until such time that we get a connection!
	 */
	for (;;) {
		pSocket new;

		/* wait for a new connection */
		new = sock_accept(input);

		/* we have a new connection */
		if (new) {
			int begin = !0;
			time_t	last_recv = time(0);
			unsigned long int	recv_count = 0;

			/* Just process forever :-) or until the connection dies */
			for (;;) {
				unsigned	char	data[32768];
				int	si;

				/* wait up to 500ms for new data to arrive */
				mypause_fd(new->fd, 500);

				/* attempt to read the data from the socket */
				si = sock_read(new, sizeof data, data);

				if (begin && si > 0) {
					unsigned char potheader[24];
					int aok;					
			
					/* copy over the header */
					memcpy(potheader, data, 24);
			
					aok = au_head_ok(potheader);
					if (aok == 1) {
						memmove(data, &data[3], si - 24);
						si -= 24;
					} else if (aok == 0) {
						/* seems legit, we assume, lol */
					} else {
						/* not legit */
						sock_close(new);
						break;
					}
					begin = 0;
				}

				if (si > 0) {
					pcm_data_push(data, si);
#ifdef __HYPER_RATE_LIMIT__
				if (si > 0) {
					time_t  tmp_recv;
                                       	float   recv_rate;
					int	margin_diff;

					loge(LOG_DEBUG2, "master.MasterServer.si = %d", si);
					recv_count += si;

					tmp_recv = time(0) - last_recv;
					if (tmp_recv >= 10) {
						if (tmp_recv > 10) {
							margin_diff = tmp_recv - 10;
							if (margin_diff > 3) {
								loge(LOG_WARN, "The margin_diff allowance was exceeded -- refusing new data for up to 3 seconds");
								purge_timeout = time(0) + 3;
								recv_count = 0;
								last_recv = time(0);
								continue;
							}
							loge(LOG_INFO, 
								"Receive data delay exceeded by %d seconds -- calculations will use %d seconds instead!", margin_diff, 10);
							//tmp_recv = 10;
						}
						recv_rate = (((float)recv_count) / ((float)tmp_recv));
						loge(LOG_DEBUG2,
							"Current master recv rate = %f, total_count = %d, diff_seconds = %d", recv_rate, recv_count, tmp_recv);
						recv_count = 0;
						last_recv = time(0);
						if (recv_rate > 3500) {
							loge(LOG_WARN, "Receive rate exceeded.  Purging all data (denying new data) for 10 seconds");
							purge_timeout = time(0) + 10;
						}
					}
					if (purge_timeout < time(0)) {
						pcm_data_push(data, si);
					}
#endif
				} else if (si < 0) {
					sock_close(new);
					break;
				}
			}
		}
	}
	return NULL;
}

/*
 * Pushes PCM data out to all the connected distribution servers
 */
int	pcm_data_push(void *data, int length) {
	pThreads	scan;

	MemLock();
	
	for (scan = AllThreadsHead; scan; scan = scan->next) {
		if (!IsStatusStream(scan))
			continue;
		scan->rbuf = bytes_append(scan->rbuf, data, length);
	}
	
	MemUnlock();

	return 0;
}

/*
 * We expect:
 *	MAGIC = .snd
 *	ENCODING = 3
 *	SAMPLE_RATE = 22050
 *	CHANNELS = 1
 *
 */
int	au_head_ok(const unsigned char *p) {
	AUFormat	au;

	au.magic = *(int *)&p[0];

	/* expect .snd in big endian */
	//if (au.magic == 0x2e736e64) {
	if (au.magic == 0x646e732e) {
		au.data_offset = ENDIANSWAP( *((int *)&p[4]) );
		au.data_size = ENDIANSWAP( *(int *)&p[8] );
		au.encoding = ENDIANSWAP( *(int *)&p[12] );
		au.sample_rate = ENDIANSWAP( *(int *)&p[16] );
		au.channels = ENDIANSWAP( *(int *)&p[20] );

		//printf("channels = %d, encoding = %d, sample_rate = %d\n", au.channels, au.encoding, au.sample_rate);	
		if (au.channels == 1 && au.encoding == 3 && au.sample_rate == 22050)
			return 1;
		return -1;
	}
	return 0;
}

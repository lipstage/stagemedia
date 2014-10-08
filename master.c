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
	AUFormat	au;

	au.channels = 1;
	au.data_offset = -1;
	au.data_size = -1;	
	au.encoding = 3;
	au.sample_rate = 22050;

	/* bind to the address and port */
	input = bind_address("127.0.0.1", 18100);
	if (input < 0) {
		fprintf(stderr, "mode=master; Could not bind to master addresses and/or ports [for recorder]\n");
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
					//printf("aok = %d\n", aok);
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
//		void	*temp;

		if (!IsStatusStream(scan))
			continue;
		scan->rbuf = bytes_append(scan->rbuf, data, length);
/*
		if (!(temp = realloc(scan->rdb.buf, scan->rdb.si + length))) {
			// handle this later
		}
		if (temp != scan->rdb.buf)
			scan->rdb.buf = temp;
		memcpy( &((unsigned char *)scan->rdb.buf)[scan->rdb.si], ((unsigned char *)data), length);
		scan->rdb.si += length;
*/
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

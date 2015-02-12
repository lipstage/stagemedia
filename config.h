#ifndef	__CONFIG_H__
#define	__CONFIG_H__

#include "version.h"

/*
 * Applies to: Master Mode Only
 *
 * The maximum size for data involved to distro servers.
 *
 * Once this size is reached, the distro server will be removed, it is really
 * that simple.
 */
#define	DISTRO_MAX_SIZE		(1024*1024*4)

/*
 * Applies to: Distribution Server
 *
 * The maximum size of a client's buffer.  Once this size is reached, no PCM data will be
 * "pushed" to the client.  
 */
#define	CLIENT_MAX_BUFSIZE	(1024*1024*4)

/*
 * The maximum number of distribution servers which can be connected to the master
 * server.  Keep in mind this is for master mode only.
 */
#define MASTER_MAX_CLIENTS	20

/*
 * Whether or not we support MP3 in requests.  Just leave this on all the time pretty much
 */
#define	SUPPORT_MP3

/*
 * The default, maximum time in seconds to wait for a socket to complete the HTTP request
 * from the server.  After this time is up, the connection will simply be discontinued.
 *
 * Keep in mind, this should NOT take long!  A request should normally be completed in under
 * about 10 seconds.
 */
#define HTTP_CLIENT_MAX_TIME		10

/*** Probably not change these anyway ***/
#define HTTP_SERVER_NAME	"StageMedia"

#define HTTP_SERVER_VERSION	STAGEMEDIA_VERSION

/* 
 * The maximum one part of the buffer will be transcoded at a time.
 * This keeps calls to calloc() which request larger sums of memory low
 */
#define	MAX_TRANSCODE_CLIP_SIZE		1024*32

/*
 * If you enable this, it will use calloc()/malloc() to create a reserve
 * buffer region for the encoding of PCM into audio for each stream.
 *
 * This should no longer be necessary as a variable is created with the
 * size of MAX_TRANSCODE_CLIP_SIZE.  
 *
 * Consider this deprecated.
 */
#undef	ALLOCATE_AUDIO_ENCODING_BUFFER

/*
 * The default configuration file we're going to load
 */
#define CONF_FILE	"stagemedia.conf"

/*
 * Default time we will wait for a stream to start.  For example, once
 * we have the client connect, we will loop in a thread; we wait for
 * data to be delivered.  However, if data is not delivered in N time 
 * (this time), the client will finish and the task will end.
 *
 * Can be overwritten with the option ``max_stream_time'' in the distro
 * configuration.  A time of 0 means wait forever.
 *
 * Default: 180 seconds (3 minutes)
 */
#define	MAX_STREAM_TIME		180	

/*
 * The following are default settings used for bindings.  In the configuration,
 * they can be overwritten using their respective configuration parameters.
 *
 * config_option		default
 * master_bind_distro_ip	MASTER_BIND_DISTRO_IP
 * master_bind_distro_port	MASTER_BIND_DISTRO_PORT
 * master_bind_recv_ip		MASTER_BIND_RECV_IP
 * master_bind_recv_port	MASTER_BIND_RECV_PORT
 */
#define	MASTER_BIND_DISTRO_IP		"0.0.0.0"		/* The IP distribution servers will use to connect to (all 0's = every IP) */
#define	MASTER_BIND_DISTRO_PORT		18001			/* The port distribution servers will connect to */
#define	MASTER_BIND_RECV_IP		"127.0.0.1"		/* The IP address the recorder (probably a web interface or similar) will connect to;
								   gets PCM data.
								*/
#define	MASTER_BIND_RECV_PORT		18000			/* The port the recorder will connect to */


/*
 * These are only valid in distribution mode and are the IP address
 * and port the distribution server will bind to.  These can be
 * over-written with their respective configuration parameters.
 *
 * config_option		default
 * distro_bind_ip		DISTRO_BIND_IP
 * distro_bind_port		DISTRO_BIND_PORT
 */
#define	DISTRO_BIND_IP			"0.0.0.0"
#define	DISTRO_BIND_PORT		8055

#define	DISTRO_CONNECT_IP		"127.0.0.1"
#define	DISTRO_CONNECT_PORT		18001

#endif

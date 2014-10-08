#ifndef	__CONFIG_H__
#define	__CONFIG_H__

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

#define HTTP_SERVER_VERSION	"0.54"

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

#endif

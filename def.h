/*
 * You should probably not modify any of the macros in this file
 * unless you're a developer and understand what they do in code.
 */
#ifndef	__DEF_H__
#define __DEF_H__

/*
 * ``bytes'' will use realloc. If you undefine this, it uses calloc. This should
 * reduce overall CPU usage :)
 */
#define	BYTES_APPEND_USE_REALLOC

/*
 * ``byte'' will use realloc instead of calloc on buffers
 */
#define BYTES_SQUASH_USE_REALLOC

/*
 * INIT_BURST_ON_CONNECT
 *
 * This is designed to "hold" about 3 seconds of audio and then send it to the
 * client's buffer for processing. If this is ONLY enabled, it will make the client
 * wait for 3 seconds before they start receiving data.
 */
#define INIT_BURST_ON_CONNECT

/*
 * You're required to enable INIT_BURST_ON_CONNECT for this to work!
 *
 * Instead of waiting for 3 seconds, the distribution server maintains an internal
 * buffer. It will simply push this internal buffer (also about 3 seconds) directly
 * into the client's buffer. It makes the playback almost instant.
 */
#define INIT_BURST_FLASHBACK

#endif /* __DEF_H__ */

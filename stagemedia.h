#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sndfile.h>
#include <sys/time.h>
#include <unistd.h>
#include <lame/lame.h>
#include <curl/curl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netdb.h>
#include <signal.h>
#include <math.h>
#include <jansson.h>

#include "mem.h"
#include "cfg.h"
#include "au.h"
#include "http.h"
#include "bytes.h"
#include "control.h"
#include "timing.h"
#include "distro.h"
#include "inject.h"
#include "api.h"
#include "log.h"
#include "config.h"
#include "version.h"

#ifdef	SUPPORT_MP3
# include  <lame/lame.h>
# include "encoder_mp3.h"
#endif

#ifndef	__SLINGMEDIA_H__
#define __SLINGMEDIA_H__

/*
 * Socket Header Information
 */
#define BUFSIZE         65536
#define MAXBUFFER	(4*1024*1024)           /* no buffer > 4 Megabytes */

#define SOCK_FLAG_SERVER	1
#define	SOCK_FLAG_CLIENT	2

#define	IsClient(s)		((s->flag & SOCK_FLAG_CLIENT))
#define	IsServer(s)		((s->flag & SOCK_FLAG_SERVER))

#define IsDeadSocket(p)         ((p->deadsocket))

#define	SOCK_FD(x)		((x)->fd)

typedef struct {
        int             deadsocket;
        int             fd;
        struct  sockaddr_in     sin;
        unsigned char   *in_buf, *out_buf;
        int     bpos;
	unsigned int	flag;
	
	char	ip_addr_string[256];
	char	port_string[256];
} Socket, *pSocket;

extern	int     bind_address(char *theaddr, ushort);
extern	pSocket sock_accept(int);
extern	pSocket	sock_connect(char *, unsigned short);
extern	int     sock_read_into_buffer(pSocket, int);
extern	int     sock_read_from_buffer(pSocket, int, void *);
extern	int     sock_read(pSocket, int, void *);
extern	void    sock_close(pSocket);
extern	int	sock_readline(char *, size_t, pSocket);
extern	int	sock_nonblock(pSocket);
extern	int	sock_block(pSocket);

/*
 * Encoding Header
 */
typedef struct {
	unsigned char	*buffer;
	int		si;
} EncodingBuffer;

typedef unsigned int    bit32;

typedef struct st_AUFormat {
        bit32   magic;
        bit32   data_offset;
        bit32   data_size;
        bit32   encoding;
        bit32   sample_rate;
        bit32   channels;
} AUFormat;

#define ES16(s)                 ((s >> 8) & 0x00ff) | ((s & 0x00ff) << 8)
#define ENDIANSWAP(num)         ( ((num >> 24) & 0xff) | ((num << 8) & 0xff0000) | ((num >> 8) & 0xff00) | ((num << 24) & 0xff000000 ))

extern EncodingBuffer	eb;

extern int	encoding_init();
extern void	*encoding();
extern void	*encoding_mp3();
extern	int	pcm_input(void *, int);

#ifdef __USE_OGG__
extern	sf_count_t	my_sf_seek();
extern	sf_count_t	my_sf_read();
extern	sf_count_t	my_sf_read_source();
extern	sf_count_t	my_sf_write();
extern	sf_count_t	my_sf_write_source();
extern	sf_count_t	my_sf_tell();
extern	sf_count_t	my_sf_get_filelen();
#else
extern int my_pcm_read(void *, int);
#endif

/*
 * Shout Header
 */
typedef struct {
	unsigned char 	*buffer;
	int		si;
} ShoutBuffer;

extern 	int	my_shout_init();
extern	int	my_shout_send();
extern	int	my_shout_end();

extern	void	*shouter();

extern	void	*MasterServer();
extern	int	au_head_ok(const unsigned char *);

extern	void	mypause();
extern	void	mypause_time();
extern	void	mypause_fd(int, int);

#endif /* __SLINGMEDIA_H__ */

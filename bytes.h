#ifndef	__BYTES_H__
#define	__BYTES_H__

#define	BYTES_TYPE_DEFAULT	0		/* Just a linear buffer */
#define BYTES_TYPE_RING		1		/* A ring buffer */

#define	IsBytesDefault(n)	((n->type == BYTES_TYPE_DEFAULT))
#define	IsBytesRing(n)		((n->type == BYTES_TYPE_RING))


typedef	struct st_bytes {
	void	*d;			/* pointer to the data */
	int	s;			/* size of the buffer itself */
	int	type;			/* options for the Bytes buffer */
	int	max_size;		/* The maximum size permitted to grow.  The behavior depends a bit on mode */
#ifdef BYTES_MUTEX
	pthread_mutex_t	lock;
#endif
} Bytes, *pBytes;

extern 	pBytes	bytes_init(void);
extern	pBytes	bytes_prefix(pBytes, void *, int);
extern	pBytes	bytes_append(pBytes, void *, int);
extern	int	bytes_peek(pBytes, void *, int);
extern	int	bytes_squash(pBytes, int);
extern	void	bytes_free(pBytes);
extern	int	bytes_size(pBytes);
extern	pBytes	bytes_copy(pBytes, pBytes, int);
extern	int	bytes_extract(pBytes, void *, int);
extern	void	*bytes_data(pBytes);
extern	int	bytes_ready(pBytes);
extern	void	bytes_purge(pBytes);


extern	void	bytes_type_set(pBytes, int );
extern	void	bytes_maxsize_set(pBytes, int);


#endif 

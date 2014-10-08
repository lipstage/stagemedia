#ifndef	__ENCODER_MP3_H__
#define	__ENCODER_MP3_H__

#include "stagemedia.h"

#define MP3_SIZE	/* 16384 */ 65536


typedef	struct	st_EncMP3 {
	lame_t	gfp;
	unsigned char obuffer[MP3_SIZE];
	
} EncMP3, *pEncMP3;

extern	EncMP3	encode_mp3_init(AUFormat);
extern	int	encode_mp3_data(EncMP3, short int *, int, int (*)(), void *);
extern	void	encode_mp3_close(EncMP3);

#endif

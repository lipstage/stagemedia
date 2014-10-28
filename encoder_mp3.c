#include "stagemedia.h"

#ifdef	SUPPORT_MP3
EncodingBuffer eb;
pthread_mutex_t         enc = PTHREAD_MUTEX_INITIALIZER;

EncMP3	encode_mp3_init(AUFormat au) {
	EncMP3	encode;

	encode.gfp = lame_init();

	lame_set_in_samplerate(encode.gfp, au.sample_rate);
	lame_set_out_samplerate(encode.gfp, 44100); 
	lame_set_num_channels(encode.gfp, au.channels);
	lame_set_quality(encode.gfp, 7);
	/* was vbr_default */
	//lame_set_VBR(encode.gfp, vbr_off);

	lame_set_VBR(encode.gfp, vbr_mt);
	lame_set_VBR_quality(encode.gfp, 7);

	lame_set_mode(encode.gfp, MONO);

	if (!(lame_init_params(encode.gfp) >= 0)) {
		loge(LOG_CRITICAL, "Lame encoding error occured, reason unknown.");
		exit(-1);
	}

	return encode;
}


int	encode_mp3_data(
	EncMP3 		encode, 
	short int 	*pcm_data, 
	int		pcm_size, 
	int 		(*write_callback)(),
	void		*callbackid
) {
	int	ret, nr, i, rc = 0;
	short 	rd;

	for (i = 0; i < pcm_size; i++) {

		rd = ES16(pcm_data[i]);

		if ((ret = lame_encode_buffer(encode.gfp, &rd, NULL, 1, encode.obuffer, sizeof encode.obuffer)) > 0) {
			nr = write_callback(callbackid, encode.obuffer, ret);
			if (nr < 0)
				return nr;
			rc += nr;
		} else if (ret < 0) {
			/* something done blown up -- need to handle this */
		} 
	}

	return rc;	
}

void	encode_mp3_close(EncMP3 m) {
	lame_close(m.gfp);
}

#endif

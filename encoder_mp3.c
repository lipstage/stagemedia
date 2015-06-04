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

	if (au.channels == 2) {
		lame_set_mode(encode.gfp, STEREO);
		encode.mode = ENCODER_MODE_STEREO;
	} else {
		lame_set_mode(encode.gfp, MONO);
		encode.mode = ENCODER_MODE_MONO;
	}

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
#ifdef MP3_ENCODER_SINGLE_SHORT
	short 	rd;
#else
	int	processed = 0;
#endif

	/*
	 * We could probably write the source a little differently here, 
	 * but it is a bit easier to follow if we don't quite do that :)
	 */
	if (encode.mode == ENCODER_MODE_MONO) {
#ifdef	MP3_ENCODER_SINGLE_SHORT
		for (i = 0; i < pcm_size; i++) {
			rd = ES16(pcm_data[i]);

			if ((ret = lame_encode_buffer(encode.gfp, &rd, NULL, 1, encode.obuffer, sizeof encode.obuffer)) > 0) {
				nr = write_callback(callbackid, encode.obuffer, ret);
				if (nr < 0)
					return nr;
				rc += nr;
			} else if (ret < 0) {
				err("%s: We received %d from the encoder -- this is unexpected.", __FUNCTION__, ret);
				return -1;
			} 
		}
#else
		for (i = 0; i < pcm_size && i < (sizeof encode.obuffer) / 2; i++) {
			pcm_data[i] = ES16(pcm_data[i]);		/* endian */
			processed++;
		}

		if ((ret = lame_encode_buffer(encode.gfp, pcm_data, pcm_data, processed, encode.obuffer, sizeof encode.obuffer)) > 0) {
			nr = write_callback(callbackid, encode.obuffer, ret);
			if (nr < 0)
				return nr;
			rc += nr;
		} else if (ret < 0) {
			err("%s: We received %d from the encoder -- this is unexpected.", __FUNCTION__, ret);
			return -1;
		}
		
		if (processed < pcm_size)
			return encode_mp3_data(encode, &pcm_data[processed], pcm_size - processed, write_callback, callbackid);
#endif
	} else if (encode.mode == ENCODER_MODE_STEREO) {
#ifdef  MP3_ENCODER_SINGLE_SHORT
		short	left, right;

		for (i = 0; i < pcm_size; i += 2) {
			left = ES16(pcm_data[i]);
			right = ES16(pcm_data[i+1]);
			
			if ((ret = lame_encode_buffer(encode.gfp, &left, &right, 1, encode.obuffer, sizeof encode.obuffer)) > 0) {
				nr = write_callback(callbackid, encode.obuffer, ret);
				if (nr < 0)
					return nr;
				rc += nr;
			} else if (ret < 0) {
				err("%s: We received %d from the encoder -- this is unexpected.", __FUNCTION__, ret);
				return -1;
			}
		}
#else
		for (i = 0; i < pcm_size && i < (sizeof encode.obuffer) / 2; i += 2) {
			pcm_data[i] = ES16(pcm_data[i]);
			pcm_data[i+1] = ES16(pcm_data[i+1]);
			processed += 2;
		}

		if ((ret = lame_encode_buffer_interleaved(encode.gfp, pcm_data, (processed / 2), encode.obuffer, sizeof encode.obuffer)) > 0) {
			nr = write_callback(callbackid, encode.obuffer, ret);
			if (nr < 0)
				return nr;
			rc += nr;
		} else if (ret < 0) {
			err("%s: We received %d from the encoder -- this is unexpected.", __FUNCTION__, ret);
			return -1;
		}

		if (processed < pcm_size)
			return encode_mp3_data(encode, &pcm_data[processed], pcm_size - processed, write_callback, callbackid);
#endif
	}

	return rc;	
}

void	encode_mp3_close(EncMP3 m) {
	lame_close(m.gfp);
}

#endif

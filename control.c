/** \file */
#include "stagemedia.h"

/*
 * This places data in the format of the StageCOM Protocol.
 * The first 1k of the stream is the "Primer" and contains, at least,
 * the following information:
 *
 * 4 byte: STcM - Magic for the stream, always STcM
 * 1 byte: Version - Next byte is the version (currently only supports 1)
 * 2 byte: Server ID
 * REMAIN: 0 padding
 */
int	Ctrl_Primer(unsigned short int id, char *buffer, size_t s, const char *server_password, char *salt) {
	/* primer is always 1k in length */
	unsigned char	data[1024] = { 0 };
	unsigned short options = 0;
	const	char	*quality;

	/* Copy over the magic for the primer */
	memcpy(data, "STcM", 4);

	/* Version is always 1 (right now) */
	data[4] = 1;

	/* We are going to use a server_password and should do something here */
	if (server_password) {
		/* no real effect, just for logic's sake really -- set the password required */
		options |= (1 << 0);

		/* create a random string */
		random_string(salt, 8);

		/* copy over the salt, too */
		memcpy(&data[7], salt, 8);
	}

	/* compression is always off for now */
	options |= (0 << 1);

	quality = cfg_read_key_df("quality", DEFAULT_QUALITY);
	if (!strcasecmp(quality, "good")) 
		options |= (QUALITY_GOOD << 2);
	else if(!strcasecmp(quality, "verygood"))
		options |= (QUALITY_VERYGOOD << 2);
	else /* acceptable */
		options |= (QUALITY_ACCEPTABLE << 2);

	/* copy over the options */
	memcpy(&data[5], &options, sizeof options);

	/* Copy over the id of our server */
	memcpy(&data[15], &id, sizeof id);

	if (s < 1024)
		return 0;

	/* copy it over */
	memcpy(buffer, data, 1024);

	/* Now we send it out */
	return 1024;
}

int	Ctrl_UnPrime(unsigned char *buffer, pCtrlPrimerHeader ctrl_header) {
	short int	val;
	unsigned short int		options;

	if (buffer[0] == 'S' && buffer[1] == 'T' && buffer[2] == 'c' && buffer[3] == 'M') {
		if (buffer[4] != 1) {
			loge(LOG_ERR, "Ctrl_UnPrime: Unknown version received from primer");
			return -1;
		}
		val = buffer[4];

		ctrl_header->version = val;
		options = *(unsigned short int *)&buffer[5];
		ctrl_header->password_protected = options & 1;
		ctrl_header->compression_enabled = options & 2;
		ctrl_header->quality = (options >> 2) & 15;

		/* Either way, we zero the salt out */
		memset(ctrl_header->salt, 0, sizeof ctrl_header->salt);

		/* Only perform the copy if it is set to happen */
		if (ctrl_header->password_protected)
			memcpy(ctrl_header->salt, &buffer[7], 8);

		return val;
	}
	return -1;
}

/*
 * Put actual data, such as PCM data into a Control Channel
 * encapsulator :P
 *
 * 4 byte: Header Block
 *	  A           B    C
 *	|------------|----|----------------|
 *	A (12 bits): 3982 (magic for header)
 *	B (4 bits) : Type of datagram being sent
 *		1: Audio (PCM 16-bit)
 *		2: Control Data (or event)		
 *	C (16 bits): The size of the datagram
 */
pBytes	Ctrl_Raw(int type, void *data, unsigned short length) {
	unsigned int	block;
	unsigned char	*ret;
	pBytes		myret;

	/* Just a little magic for this block */
	block = 3982;
	block <<= 20;
	block |= ((type & 0xf) << 16);
	block |= (length & 0xffff);

	/* This is where we write it out, along with the data and length */
	if (!(ret = calloc(1, length + sizeof block)))
		return NULL;

	/* copy over the first piece */
	memcpy(ret, &block, sizeof block);

	/* copy the rest */
	memcpy(&ret[4], data, length);

	if (!(myret = calloc(1, sizeof(*myret)))) {
		free (ret);
		return NULL;
	}
	myret->d = ret;
	myret->s = sizeof block + length;

	return myret;
}

pBytes	Ctrl_Datum(void *data, unsigned short length) {
	return Ctrl_Raw(SMPL_AUDIO, data, length);
}

int	Ctrl_Unpack(pCtrlData ct, void *data, int maxlimit) {
	unsigned int	hd;

	/* Must have a good size to deal with some stuff */
	if (maxlimit < sizeof(hd))
		return 0;

	/* subtract to make it easier to read later */
	maxlimit -= sizeof(hd);

	/* We point to the header portion, which is the first 4 bytes */
	hd = *((unsigned int *)data);

	/* We expect to find our magic :) */
	if (((hd & 0xfff00000) >> 20) == 3982) {
		int	type;
		int	size;

		/* get the actual size of the data */
		size = (hd & 0xffff);

		/* The size should *not* be larger than maxlimit, otherwise, we should wait */
		if (size > maxlimit)
			return 0;

		/* fetch the type associated with the datagram */
		type = (hd >> 16) & 0xf;

		switch(type) {
			int	dsize;

			/*
			 * Normal PCM payload
			 */
			case SMPL_AUDIO:

				dsize = hd & 0xffff;
				//if (dsize > maxlimit)
				//	return 0;

				ct->type = SMPL_AUDIO;
				ct->dsize = dsize;

				if (!(ct->data = calloc(1, ct->dsize)))
					;	/* need to handle this better */
				memcpy(ct->data, &((unsigned char *)data)[4], ct->dsize);

				/* Return total number of bytes in scope */
				return ct->dsize + sizeof(hd);
			case SMPL_CONTROL:
				break;
			default:	/* huh? */
				break;
		}
	}
	return 0;
}

void Ctrl_UnPack_Free(pCtrlData s) {
	if (!s)
		return;
	if (s->data != NULL)
		free (s->data);
	s->data = NULL;
	s->dsize = 0;
}

void Ctrl_Pack_Free(pBytes s) {
	if (s) {
		if (s->d) {
			free(s->d);
			s->d = NULL;
			s->s = 0;
		}
		free (s);
	}
}



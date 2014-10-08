#include "stagemedia.h"

pBytes	bytes_init(void) {
	pBytes	new;

	if (!(new = calloc(1, sizeof *new)))
		return NULL;

	new->d = NULL;
	new->s = 0;
	new->type = BYTES_TYPE_DEFAULT;
	new->max_size = 0;	

	return (new);
}

/*
 * Adds or "tacks" on new content to an existing Bytes structure.
 */
pBytes	bytes_append(pBytes in, void *data, int size) {
	pBytes	out = NULL;

	/* that just is nonsense */
	if (size < 0) {
		fprintf(stderr, "I RECEIVED NONSENSE!!!!!! %d\n", size);
		exit(-1);
		return in;
	}

	if (!size)
		return (in != NULL ? in : NULL);

	/*
	 * If it already exists
	 */
	if (in) {
		out = in;

		/*
		 * This is the first time we've inserted any data into this buffer.
		 * Actually, we do NOT take ring into account in this case.
		 */
		//printf("Step 1 %p %d\n", in->d, in->s);fflush(stdout);
		
		if (!out->d) {
			if (!(out->d = calloc(1, size)))
				return NULL;
			memcpy(out->d, data, size);
			out->s = size;
			return out;
		} else {
			void 	*temp;
			int	newsize;

			/* calculate the new size required */
			newsize = (out->s + size);

			/*
			 * The buffer is a type ring, which means we only allow a certain size of data
			 * to exist.  Any data which is older gets "chopped" off from the front, making way
			 * for newer data.
			 */
			if (IsBytesRing(out) && out->max_size < newsize) {
				int diff_size = newsize - out->max_size;

				/* copy the data up and remove some of the existing data -- yes, this is a "ring" */
				memmove( (char *)out->d, &((char *)out->d)[diff_size], out->s - diff_size);

				/* update the size as appropriate */
				out->s -= diff_size;

				/* update to the new size */
				newsize -= diff_size;

				/* now, we simply allow it to "fall through", which should more or less autocorrect */
			}

			/* 
			 * We just allocate an entirely new area instead of using realloc(). 
			 * If this should fail, then return back the 'in' pointer -- if we
			 * return NULL, then we leak memory.
			 */
			if ((temp = calloc(1, newsize))) {
				char	*temp_string = temp;
		
				/* copy over the current data */
				memmove( (char *) temp, (char *) out->d, out->s);

				/* append our new data, that we're trying to add */
				memmove( &temp_string[out->s], (char *)data, size);

				/* zero out the current, soon to be unallocated memory (probably overkill) */
				memset(out->d, 0, out->s);

				/* free the current memory area */
				free(out->d);

				/* update our size and location of the new memory areas! */
				out->s = newsize;
				out->d = temp;
			} else
				return in;
		}
	} else {
		/*
		 * If our 'in' pointer was NULL, then we allocate enough memory to hold it.
		 * After that, we just re-run outselves with the in pointer as the first argument.
		 * In that case, it should just append it.
		 */
		if ((out = calloc(1, sizeof(*out)))) {
			out->d = NULL;
			out->s = 0;
			return bytes_append(out, data, size);
		}
	}

	return out;
}


/*
 * Does *not* remove data from the top of a Bytes structure.  However,
 * it does return that data, up to a certain number of bytes.  This allows
 * you to read what's there before destroying anything.
 * 
 */
int	bytes_peek(pBytes in, void *buffer, int s) {
	int	size;

	if (!in || !in->d)
		return -1;

	/* get the appropriate (real) size we're going to use */
	size = (in->s < s ? in->s : s);

	/* If the size is 0, don't bother */
	if (size <= 0)
		return 0;

	/* copy the data over */
	memcpy( (unsigned char *)buffer, in->d, size);

	return size;
}

/*
 * Just pull of ``s'' size of bytes from the beginning of the data
 * stream and remove the extra
 */
int	bytes_extract(pBytes in, void *buffer, int s) {
	int	ret;

	/* attempt to perform that initial copy */
	if ((ret = bytes_peek(in, buffer, s)) <= 0)
		return ret;

	/* remove a little something from the top */
	bytes_squash(in, ret);

	return ret;
}

/*
 * Simply removes the first N bytes from the beginning and re-calculates
 * the memory usage.  
 */
int	bytes_squash(pBytes in, int n) {
	void 	*temp;
	int	newsize;	

	if (!in || !in->d)
		//return -1;
		return 0;

	if (in->s < n) {
		//return -1;
		fprintf(stderr, "EXPLAIN TO ME WHAT HAPPEND\n");
		exit(1);
		return 0;
	}

	if (!n)
		return 0;

	newsize = in->s - n;

	/*
	 * If the new size is going to be 0, then free it up completely.
	 */
	if (!newsize) {
		if (in && in->d) {
			free (in->d);
			in->d = NULL;
			in->s = 0;
		}
		return 0;
	}

	/* allocate new memory area -- yes, we could have used realloc, but whatevs */
	if ((temp = calloc(1, newsize))) {

		/* copy the data over */
		memmove(temp, &((unsigned char *)in->d)[n], newsize);

		/* free the original d, then update the memory location */
		free (in->d);
		in->d = temp;

		/* set the new size that we have! */
		in->s = newsize;

		return 0;	/* success */
	}
	return -1;
}

/*
 * Free the data within a pBytes
 */
void	bytes_free(pBytes in) {
	if (in && in->d) {
		memset(in->d, 0, in->s);
		free (in->d);
		in->d = NULL;
		in->s = 0;
		
		free (in);
		return;
	}

	if (in)
		free(in);
}

/*
 * Get the size of the data 
 */
int	bytes_size(pBytes in) {
	if (in)
		return (in->s >= 0 ? in->s : 0);
	return 0;
}

/*
 * Get the bytes data area
 */
void	*bytes_data(pBytes in) {
	if (in)
		return in->d;
	return NULL;
}

int	bytes_ready(pBytes in) {
	return (bytes_size(in) > 0);
}


void	bytes_type_set(pBytes in, int n) {
	if (in) 
		in->type = n;
}

void	bytes_maxsize_set(pBytes in, int n) {
	if (in)
		in->max_size = n;
}

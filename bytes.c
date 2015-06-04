#include "stagemedia.h"

pBytes	bytes_init(void) {
	pBytes	new;

	if (!(new = calloc(1, sizeof *new)))
		return NULL;

	new->d = NULL;
	new->s = 0;
	new->type = BYTES_TYPE_DEFAULT;
	new->max_size = 0;	
#ifdef BYTES_MUTEX
	pthread_mutex_init(&new->lock, NULL);
#endif

	return (new);
}

/*
 * Write bytes to the FRONT instead of the end. Oh...this doesn't take
 * into account anything about ring types right now, since it assumes..
 * if you're doing this: Something bad happened and you need to save stuff.
 */
pBytes	bytes_prefix(pBytes in, void *data, int size) {
	if (size < 0) {
		loge(LOG_CRITICAL, "bytes_prefix: Returned size of less than 0.  Expected behavior size=%d", size);
		exit(-1);
		return in;
        }

	if (!size)
		return (in != NULL ? in : NULL);
	
	if (in) {
		pBytes	temp;
		int	newsize;

#ifdef BYTES_MUTEX
		bytes_lock(in);
#endif
		if (!in->d) {
#ifdef BYTES_MUTEX
			bytes_unlock(in);
#endif
			return bytes_append(in, data, size);
		}
		newsize = size + in->s;
		if ((temp = realloc(data, newsize))) {

			/* set it to the new data area */
			in->d = temp;

			/* push the data back! */
			memmove(&((unsigned char *)in->d)[size], in->d, in->s);

			/* now put the new data up front */
			memmove(in->d, data, size);

			/* update the size */
			in->s = newsize;

			/* return in via fall-through */
		}
#ifdef BYTES_MUTEX
		bytes_unlock(in);
#endif
		return in;		/* this should be handled MUCH better */
	}
	return bytes_append(in, data, size);	/* already written a lot of code to handle this */
}

/*
 * Adds or "tacks" on new content to an existing Bytes structure.
 */
pBytes	bytes_append(pBytes in, void *data, int size) {
	pBytes	out = NULL;

	/* that just is nonsense */
	if (size < 0) {
		loge(LOG_CRITICAL, "bytes_append: Returned size of less than 0.  Expected behavior size=%d", size);
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

#ifdef BYTES_MUTEX
		bytes_lock(in);
#endif
		/*
		 * This is the first time we've inserted any data into this buffer.
		 * Actually, we do NOT take ring into account in this case.
		 */
		if (!out->d) {
			if (!(out->d = calloc(1, size)))
				return NULL;
			memcpy(out->d, data, size);
			out->s = size;
#ifdef BYTES_MUTEX
			bytes_unlock(out);
#endif
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

#ifdef	BYTES_APPEND_USE_REALLOC
			/*
			 * Attempt to allocate the new size, which should be larger :)
			 */
			if ((temp = realloc(out->d, newsize)) != NULL) {
				/* If it isn't NULL, it should be safe to just set it */
				out->d = temp;
				
				/* Append our data to the end of what's already there */
				memmove( &((char *)out->d)[out->s], (char *) data, size);

				/* And we have a new size */
				out->s = newsize;
			} else {
#ifdef BYTES_MUTEX
				bytes_unlock(in);
#endif
				return in;
			}
#else
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
			} else {
#ifdef BYTES_MUTEX
				bytes_unlock(in);
#endif
				return in;
			}
#endif
		}
#ifdef BYTES_MUTEX
		bytes_unlock(in);
#endif
	} else {
		/*
		 * If our 'in' pointer was NULL, then we allocate enough memory to hold it.
		 * After that, we just re-run ourselves with the in pointer as the first argument.
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
 * Transfer all bytes from 'src' to 'dest'
 *
 * Does NOT erase src :)
 */
pBytes	bytes_copy(pBytes dest, pBytes src, int maxsize) {
	if (!dest || !src || !src->d)
		return (dest);
	if (maxsize <= 0)
		maxsize = src->s;
	if (src->s > 0)
		return bytes_append(dest, src->d, (src->s < maxsize ? src->s : maxsize));
	return (dest);
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
		return 0;

	if (in->s < n) {
		loge(LOG_CRITICAL, "bytes_squash: Input bytes is less than n.  Unexpected behavior");
		exit(-1);
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

#ifdef BYTES_SQUASH_USE_REALLOC	
	/* move the data backwards */
	memmove (in->d, &((unsigned char *)in->d)[n], newsize);

	/* attempt to strink the memory area */
	if ((temp = realloc(in->d, newsize)) != NULL) {
		/* Just set it to the new location */
		in->d = temp;

		/* Set the new size */
		in->s = newsize;

		return 0; /* success */
	}
#else
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
#endif
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
#ifdef BYTES_MUTEX
		pthread_mutex_destroy(&new->lock);
#endif
		return;
	}

	if (in) {
#ifdef BYTES_MUTEX
		phthread_mutex_destroy(&new->lock);
#endif
		free(in);
	}
}

void	bytes_purge(pBytes in) {
	if (in && in->d) {
		free (in->d);
		in->d = NULL;
		in->s = 0;
	}
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

#ifdef BYTES_MUTEX
int	bytes_lockop(pBytes p, int amlocking) {
	if (amlocking) {
		pthread_mutex_lock(&p->lock);
	} else {
		pthread_mutex_unlock(&p->lock);
	}
	return 0;
}

int	bytes_lock(pBytes p) {
	return bytes_locking(p, 1);
}

int	bytes_unlock(pBytes p) {
	return bytes_locking(p, 0);
}
#endif

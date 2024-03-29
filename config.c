#include "stagemedia.h"

pConfig ConfigHead = NULL;
pConfig TmpConfigHead = NULL;
pConfig SuperConfigHead = NULL;
static	int	version = 1;

static  pthread_mutex_t cfg_mutex = PTHREAD_MUTEX_INITIALIZER;
static  pthread_mutex_t pre_cfg_mutex = PTHREAD_MUTEX_INITIALIZER;
static	int             is_cfg_locked;
static 	pthread_t       cfg_current_lock = -1;
static	int	cfg_lock(void),	cfg_unlock(void);

/*
 * Reads the file and loads the user-defined configuration
 */
int	read_config(const char *filename, int isboot) {
	FILE	*fp;
	char	buffer[1024];
	int	count = 0, doload = 0;
	const	char	*stanza;
	char	stanza_start[64], stanza_end[64];

	/* Get the superhead key -- if we have one */
	if ((stanza = cfg_read_superhead_key("__STANZA__"))) {
		snprintf(stanza_start, sizeof stanza_start,  "<%s>", stanza);
		snprintf(stanza_end, sizeof stanza_end, "</%s>", stanza);
	} else 
		doload = 1;



	/*
	 * Open the file for reading
	 */
	if (!(fp = fopen(filename, "r"))) {
		if (isboot) {
			fprintf(stderr, "no config file found\n");
			exit(-1);
		} else {
			loge(LOG_, "No configuration file found");
			return -1;
		}
	}

	/* Get a global lock */
	cfg_lock();

	while (fgets(buffer, sizeof buffer, fp)) {
		char *p, *tmp;

		count++;
		while ((p = strchr(buffer, '\r')))
			*p = '\n';
		while ((p = strchr(buffer, '\n')))
			*p = '\0';

		p = buffer;
		if (!*p)
			continue;
		if (isspace(*p))
			p++;
		if (!*p || *p == '#')
			continue;

		if (*p == '<' && p[strlen(p)-1] == '>') {
			if (!stanza) {
				if (isboot) {
					fprintf(stderr,
						"Stanza found, but no stanza specified on the command line...cannot continue :-(\n");
					exit(-1);
				} else {
					loge(LOG_CRITICAL, "Stanza found, but no stanza found on the command line.  I have to die now :-(");
					exit (-1);
				}
			}

			if (!doload && !strcmp(stanza_start, p)) 
				++doload;
			else if (doload && !strcmp(stanza_end, p)) 
				break;
			else if ( (!doload && !strcmp(stanza_end, p)) ||  (doload && strcmp(stanza_start, p)) ) {
				if (isboot) {
					fprintf(stderr, "Stanza error\n");
					exit(-1);
				} else {
					loge(LOG_CRITICAL, "You have an error in your stanzas...dying");
					exit(-1);
				}
			}
			continue;			
		}

		/* We ONLY load if the flag is set */
		if (!doload)
			continue;

		tmp = strchr(p, '=');
		if (!tmp) {
			if (isboot) {
				fprintf(stderr, "line %d: Syntax error, no ``='' found\n", count);
				exit (-1);
			} else {
				/* log our displeasure of the situation */
				loge(LOG_CRITICAL, "read_config: line %d: Syntax error, no ``='' found", count);
				
				/* remove anything we've done so far -- no leaks! */
				purge_config(&TmpConfigHead);

				/* Unlock memory */
				cfg_unlock();

				return -1;
			}
		}
		*tmp++ = '\0';
		while (isspace(p[strlen(p)-1]))
			p[strlen(p)-1] = '\0';
		while (isspace(tmp[strlen(tmp)-1]))
			tmp[strlen(tmp)-1] = '\0';
		while (isspace(*tmp))
			tmp++;
		if (!strlen(p) || !strlen(tmp))	{
			if (isboot) {
				fprintf(stderr,	"line %d: Either key or value is blank\n", count);
				exit(-1);
			} else {
				/* log our displeasure of the situation */
				loge(LOG_CRITICAL, "read_config: line %d: Either key or value is blank", count);

				/* remove anything we've done so far -- no leaks! */
				purge_config(&TmpConfigHead);

				/* Unlock memory */
				cfg_unlock();

				return -1;
			}
		}
		
		add_config(&TmpConfigHead, p, tmp);	
	}

	/*
	 * Update with the new configuration
	 */
	if (TmpConfigHead) {
		pConfig tmp;

		/* temporarily store pointer to the ConfigHead (main one) */	
		tmp = ConfigHead;

		/* This is now "live" */
		ConfigHead = TmpConfigHead;

		/* The temporary TmpConfigHead can now be made NULL */
		TmpConfigHead = NULL;

		/* purge the original temporary from earlier (previous ConfigHead) */
		purge_config(&tmp);

		/* Update our version */
		version++;
	}

	/* unlock memory */
	cfg_unlock();

	fclose(fp);

	return 0;
}

/*
 * To the specified config list, we add a specific configuration option.
 */
void	add_config(pConfig *head, const char *key, const char *value) {
	pConfig new;

	/* If we are referencing a file */
	if (!strncmp(value, "@@@", 3)) {
		const char	*tp = value + 3;

		loge(LOG_DEBUG, "add_config: Referencing external file and proceeding to load that file");
		if (*tp) {
			FILE	*lp = fopen(tp, "r");
			char	buffer[16384];

			/* If we opened the file, attempt to set it to the value */
			if (lp) {
				value = fgets(buffer, sizeof buffer - 1, lp);
				fclose(lp);

				/* log the entry */
				loge(LOG_DEBUG, "add_config: in reference, setting value for %s to %s", tp, value);
			} else {
				loge(LOG_DEBUG, "Was not able to load the file %s, setting the value to NULL - The key will not be loaded!", tp);
				value = NULL;
			}
		}
	}

	/* We must have a non-NULL value to load the key */
	if (!value) {
		loge(LOG_DEBUG, "add_config: The value was null for key ``%s'' - skipping load for key.", key);
		return;
	}

	/* This is a key that we won't add UNLESS it is superhead */
	if (head  && *head != SuperConfigHead) {
		if (!strcasecmp(key, "__STANZA__"))
			return;
	}

	if (!(new = calloc(1, sizeof *new))) {
		loge(LOG_CRITICAL, "add_config: Memory allocation error occurred during setting");
		exit(-1);
	}
	if (!*head)
		*head = new;
	else {
		pConfig scan;

		for (scan = *head; scan->next; scan = scan->next)
			;
		scan->next = new;
	}
	new->next = NULL;

	strncpy(new->key, key, sizeof new->key - 1);
	strncpy(new->value, value, sizeof new->value - 1);
}

/*
 * Add to the superhead configuration any values we get.  This is mostly used
 * for the command line parameters, as they superscede any and all configuration
 * parameters.
 */
void	superhead_config_add(const char *key, const char *value) {
	add_config(&SuperConfigHead, key, value);
}

/*
 * Removes all configuration options from the given list
 */
void	purge_config(pConfig *head) {
	pConfig temp;

	while (*head) {
		temp = *head;
		*head = (*head)->next;
		free (temp);
	}
}

/*
 * Gets a specific key from the SuperConfigHead or ConfigHead following
 * these simple rules:
 *  - SuperConfigHead is scanned first, if there's a match, we return that.
 *  - ConfigHead is scanned second, if there's a match, we return that.
 *  - All else, NULL
 */
const char * cfg_read_key(const char *key) {
	pConfig scan;

	cfg_lock();

	/* scan the superhead first, it takes priority */
	for (scan = SuperConfigHead; scan; scan = scan->next)
		if (!strcasecmp(scan->key, key)) {
			cfg_unlock();
			return scan->value;
		}

	/* scan the default from the configuration -- it takes second priority */
	for (scan = ConfigHead; scan; scan = scan->next)
		if (!strcasecmp(scan->key, key)) {
			cfg_unlock();
			return scan->value;
		}

	cfg_unlock();
	return NULL;
}

/*
 * Reads ONLY the superhead.  Otherwise, this is pretty much identical to
 * cfg_read_key()
 */
const char * cfg_read_superhead_key(const char *key) {
	pConfig	scan;
	
	cfg_lock();
	for (scan = SuperConfigHead; scan; scan = scan->next) 
		if (!strcasecmp(scan->key, key)) {
			cfg_unlock();
			return scan->value;
		}
	cfg_unlock();
	return NULL;
}


/*
 * Throw back number of configuration lines
 */
int	cfg_lines(void) {
	pConfig scan;
	int	count;

	for (scan = ConfigHead, count = 0; scan; scan = scan->next, ++count)
		;
	return count;
}


/*
 * Dumps the configuration and sends it to the log, for fun
 * and profit
 */
void	cfg_dump(void) {
	pConfig scan;

	cfg_lock();
	debug2("--- Dumping all configuration options");

	for (scan = SuperConfigHead; scan; scan = scan->next)
		debug2("--- [CMDL PARAMS] %s = %s", scan->key, scan->value);

	for (scan = ConfigHead; scan; scan = scan->next)
		debug2("--- [CONFIG FILE] %s = %s", scan->key, scan->value);

	debug2("--- End of configuration dump");

	cfg_unlock();
}

/*
 * Read a key from the default list, but use 'def' as a default
 * if it isn't already defined.
 */
const char * cfg_read_key_df(const char *key, const char *def) {
	const	char	*p;
	
	p = cfg_read_key(key);
	return (p ? p : def);
}

/*
 * Looks for 1/true or 0/false for the given key value and returns
 * non-zero if true or non-zero.  Returns 0 if false.
 */
int cfg_is_true(const char *key, int def) {
	const char *p;

	p = cfg_read_key_df(key, def ? "true" : "false");
	if (!strcasecmp(p, "true") || !strcmp(p, "0"))
		return !0;
	return 0;
}

/*
 * Get the version of our configuration
 */
int	cfg_version(void) {
	return version;
}


/*
 * If a pid file is "asked" then this is the place to call.
 *
 * We do not make one by default
 */
int	pid_file(int toopen) {
	static FILE	*pid_file;
	static char	pid_file_name[8192];
	const char	*p;

	/*
	 * If we *should* kick off a pid file.  Must be in the configuration and we
	 * can't have it already open.
	 */
	if (toopen && !pid_file) {
		/* In the config? */
		if (!(p = cfg_read_key("pid_file"))) {
			debug2("No pid file set in configuration, skipping.");
			return 0;
		}

		/* We aren't able to make the pid? */
		if (!(pid_file = fopen(p, "w"))) {
			/* log the error */
			loge(LOG_CRITICAL, "Unable to create pid file: %s", p);

			/* die :( */
			exit (-1);
		}

		/* verbose logging */		
		debug2("Opened pid_file (%s) and preparing to write our pid (%d)", p, getpid());

		/* write the pid and flush */
		fprintf(pid_file, "%d\n", getpid());
		fflush(pid_file);

		/* save the name */
		strncpy(pid_file_name, p, sizeof pid_file_name - 1);
	} else {
		/* If we opened up one previously */
		if (pid_file) {
			/* verbose */
			debug2("Closing pid_file file descriptor (%s)", pid_file_name);

			/* close the file descriptor */
			fclose(pid_file);

			/* more verose, yes */
			debug2("Deleting pid_file (%s)", pid_file_name);

			/* delete it */
			unlink(pid_file_name);

			/* zero out and clean so we can open up later */
			memset(pid_file_name, 0, sizeof pid_file_name);
			pid_file = NULL;
		} else
			loge(LOG_DEBUG2, "No pid file previously opened, skipping the close.");
		
	}

	debug2("Leaving function pid_file and returning");
	return 0;
}



/*
 */

static	int	cfg_lock(void) {
	/* Get a "pre" lock -- this prevents deadlock situations */
	pthread_mutex_lock(&pre_cfg_mutex);

	if (is_cfg_locked > 0 && cfg_current_lock == pthread_self()) {
		pthread_mutex_unlock(&pre_cfg_mutex);
		return 0;
	}

	pthread_mutex_lock(&cfg_mutex);
	cfg_current_lock = pthread_self();
	is_cfg_locked++;
	pthread_mutex_unlock(&pre_cfg_mutex);

	return 0;
}

static	int	cfg_unlock(void) {
	cfg_current_lock = -1;
	is_cfg_locked = 0;
	pthread_mutex_unlock(&cfg_mutex);
	return 0;
}

#include "stagemedia.h"

pConfig ConfigHead = NULL;
pConfig TmpConfigHead = NULL;
pConfig SuperConfigHead = NULL;
static	int	version = 1;

/*
 * Reads the file and loads the user-defined configuration
 */
int	read_config(const char *filename, int isboot) {
	FILE	*fp;
	char	buffer[1024];
	int	count = 0;

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

	/* Lock memory for reload and all */
	MemLock();

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
				MemUnlock();

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
				MemUnlock();

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
	MemUnlock();

	fclose(fp);

	return 0;
}

/*
 * To the specified config list, we add a specific configuration option.
 */
void	add_config(pConfig *head, const char *key, const char *value) {
	pConfig new;

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

	MemLock();

	/* scan the superhead first, it takes priority */
	for (scan = SuperConfigHead; scan; scan = scan->next)
		if (!strcasecmp(scan->key, key)) {
			MemUnlock();
			return scan->value;
		}

	/* scan the default from the configuration -- it takes second priority */
	for (scan = ConfigHead; scan; scan = scan->next)
		if (!strcasecmp(scan->key, key)) {
			MemUnlock();
			return scan->value;
		}
	MemUnlock();
	return NULL;
}

/*
 * Dumps the configuration and sends it to the log, for fun
 * and profit
 */
void	cfg_dump(void) {
	pConfig scan;

	MemLock();

	debug2("--- Dumping all configuration options");

	for (scan = SuperConfigHead; scan; scan = scan->next)
		debug2("--- [CMDL PARAMS] %s = %s", scan->key, scan->value);

	for (scan = ConfigHead; scan; scan = scan->next)
		debug2("--- [CONFIG FILE] %s = %s", scan->key, scan->value);

	debug2("--- End of configuration dump");

	MemUnlock();
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

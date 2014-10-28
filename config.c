#include "stagemedia.h"

pConfig ConfigHead = NULL;
pConfig TmpConfigHead = NULL;
static	int	version = 1;

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

	/* purge the config */
	/* purge_config(&ConfigHead); */

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
				fprintf(stderr, "line %d: Syntax error, no ``='' found", count);
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

void	purge_config(pConfig *head) {
	pConfig temp;

	while (*head) {
		temp = *head;
		*head = (*head)->next;
		free (temp);
	}
}

const char * cfg_read_key(const char *key) {
	pConfig scan;

	MemLock();
	for (scan = ConfigHead; scan; scan = scan->next)
		if (!strcasecmp(scan->key, key)) {
			MemUnlock();
			return scan->value;
		}
	MemUnlock();
	return NULL;
}

const char * cfg_read_key_df(const char *key, const char *def) {
	const	char	*p;
	
	p = cfg_read_key(key);
	return (p ? p : def);
}

int cfg_is_true(const char *key, int def) {
	const char *p;

	p = cfg_read_key_df(key, def ? "true" : "false");
	if (!strcasecmp(p, "true") || !strcmp(p, "0"))
		return !0;
	return 0;
}

int	cfg_version(void) {
	return version;
}

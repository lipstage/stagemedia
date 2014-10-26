#include "stagemedia.h"

pConfig ConfigHead = NULL;

void	read_config(const char *filename) {
	FILE	*fp;
	char	buffer[1024];
	int	count = 0;

	if (!(fp = fopen(filename, "r"))) {
		fprintf(stderr, "no config file found\n");
		exit(-1);
	}

	/* Lock memory for reload and all */
	MemLock();

	/* purge the config */
	purge_config();

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
			fprintf(stderr, "line %d: Syntax error, no = found!\n", count);
			exit(-1);
		}
		*tmp++ = '\0';
		while (isspace(p[strlen(p)-1]))
			p[strlen(p)-1] = '\0';
		while (isspace(tmp[strlen(tmp)-1]))
			tmp[strlen(tmp)-1] = '\0';
		while (isspace(*tmp))
			tmp++;
		if (!strlen(p) || !strlen(tmp))	{
			fprintf(stderr,	"line %d: Either key or value is blank\n", count);
			exit(-1);
		}
		
		add_config(p, tmp);	
	}

	/* unlock memory */
	MemUnlock();

	fclose(fp);
}

void	add_config(const char *key, const char *value) {

	pConfig new;

	if (!(new = calloc(1, sizeof *new))) {
		fprintf(stderr, "Memory error!\n");
		exit(-1);
	}
	if (!ConfigHead)
		ConfigHead = new;
	else {
		pConfig scan;

		for (scan = ConfigHead; scan->next; scan = scan->next)
			;
		scan->next = new;
	}
	new->next = NULL;

	strncpy(new->key, key, sizeof new->key - 1);
	strncpy(new->value, value, sizeof new->value - 1);
}

void	purge_config(void) {
	pConfig temp;

	while (ConfigHead) {
		temp = ConfigHead;
		ConfigHead = ConfigHead->next;
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

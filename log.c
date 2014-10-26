#include "stagemedia.h"
#include "config.h"

static	FILE	*logfp = NULL;
static	char	log_buffer[16384];

/*
 * Open the log file if we need to
 */
void	log_init(void) {
	const char *p = cfg_read_key("log_file");

	/* Just in case someone calls log_init() again */
	if (logfp)
		log_close();

	/* I guess logging isn't an interest */
	if (!p) 
		return;

	/* attemp to open the desired log file */
	if (!(logfp = fopen(p, "a"))) {
		fprintf(stderr, "Unable to open log \"%s\"\n", p);
		exit (-1);
	}

	/* set the buffering to line buffered */
	setvbuf(logfp, log_buffer, _IOLBF, sizeof log_buffer);

	/* it worked (probably), so write something out */
	loge(LOG_, "Log file started, pid = %d\n", getpid());
}

/*
 * Close down any existing log file which may be open
 */
void	log_close(void) {
	if (logfp && logfp != stdout) {
		loge(LOG_, "closing log");
		fclose(logfp);
	}
	logfp = NULL;
}

/*
 * Make an entry in the log
 */
void	loge(int level, const char *fmt, ...) {
	char	buffer[8192], final[8192], lname[64];
	int	max_size = sizeof(buffer) - 512;
	struct	tm	*td;
	time_t	now = time(0);
	va_list	ap;

	/*
	 * Don't waste cycles on something useless
	 */
	if (!logfp)
		return;

	/*
	 * Create the specified line item entry
	 */
	va_start(ap, fmt);
	vsnprintf(buffer, max_size, fmt, ap);
	va_end(ap);

	/*
	 * Generate the time stamp
	 */
	td = localtime(&now);
	strftime(final, 512, "[%m/%d/%Y %H:%M:%S %z %Z", td);

	/*
	 * We will now specify the logging level, right?
	 */
	if ((log_level_name(lname, sizeof lname - 1, level))) {
		strncat(final, "(", 512);
		strncat(final, lname, 512);
		strncat(final, ")]  ", 512);
	} else 
		strncat(final, "]  ", 512);

	/*
	 * Copy over the previously formatted stuff
	 */
	strncat(final, buffer, sizeof final - 1);

	/*
	 * In case we have new lines, we get rid of them.
	 */
	while (strchr(final, '\n'))
		*strchr(final, '\n') = '\0';
	while (strchr(final, '\r'))
		*strchr(final, '\r') = '\0';

	/*
	 * Write out the log entry
	 */
	fprintf(logfp, "%s\n", final);
}

char *log_level_name(char *n, int ns, int level) {
	switch (level) {
		case LOG_DEBUG:
			strncpy(n, "debug", ns);
			break;
		case LOG_INFO:
			strncpy(n, "info", ns);
			break;
		case LOG_WARN:
			strncpy(n, "warn", ns);
			break;
		case LOG_ERR:
			strncpy(n, "err", ns);
			break;
		case LOG_:
		default:
			return NULL;
			break;	/* UNREACHABLE */
	}
	return n;
}

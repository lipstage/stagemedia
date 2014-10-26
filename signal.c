#include "stagemedia.h"

static	sigset_t	signal_set;
static	pthread_t	signal_thread_id;
pthread_mutex_t sig_threadlock = PTHREAD_MUTEX_INITIALIZER;
unsigned int	sigtrack = 0;

static int sigtrack_unique_bits(unsigned int signum);

/*
 * Initialize signal handling
 *
 * We will:
 *  - Block signals, except for those arriving to a single thread
 *  - Create a single thread to accept signals via sigwait()
 */
int	signal_init(void) {
	int	reg_signals[] = {
		SIGINT,
		SIGHUP,
		SIGTERM,
		SIGUSR1,
		SIGUSR2,
		SIGPIPE,
		0
	}, i;
	static	int	hasinit = 0;

	if (hasinit) {
		loge(LOG_ERR, "signal_init() was called more than once, not sure how this happened -- ignoring");
		return 0;
	}

	sigemptyset(&signal_set);
	for (i = 0; reg_signals[i]; i++) {
		if ((sigaddset(&signal_set, reg_signals[i])) != 0) {
			loge(LOG_ERR, "Registeration of signals failed for an unknown reason -- stopping (sig: %d)", reg_signals[i]);
			perror("sigaddset");
			exit(0);
		}
	}

	/* block signals in other threads */
	pthread_sigmask(SIG_BLOCK, &signal_set, NULL);

	/* write to the log */
	loge(LOG_DEBUG, "Starting signal thread for async signals");

	/* Launch the thread signal */
	pthread_create(&signal_thread_id, NULL, signal_thread, NULL);

	return 0;
}

/*
 * Lock others from accessing sigtrack
 */
int	SigLock(void) {
	return pthread_mutex_lock(&sig_threadlock);
}

/*
 * Unlock others from accessing sigtrack
 */
int	SigUnlock(void) {
	return pthread_mutex_unlock(&sig_threadlock);
}

void	*signal_thread(void *arg) {
	int	sn;
	int	raise = 0;

	/*
	 * Another debug notice 
	 */
	loge(LOG_DEBUG, "Signal thread started and waiting to receive signals");
	
	/*
	 * Loop forever -- This naturally blocks so no sleep necessary
	 */
	for (;;) {
		/* wait for a signal */
		sigwait(&signal_set, &sn);

		/* in case we need to know */
		loge(LOG_DEBUG, "Received signal %d\n", sn);
	
		/*
		 * Quickly evaluate the received signal and set our
		 * own bit values as a component of that.
		 */
		switch(sn) {
			case SIGUSR1:
				loge(LOG_DEBUG, "Received SIGUSR1");
				raise = SIGNAL_USR1;
				break;
			case SIGUSR2:
				loge(LOG_DEBUG, "Received SIGUSR2");
				raise = SIGNAL_USR2;
				break;
			case SIGHUP:
				loge(LOG_DEBUG, "Received SIGHUP");
				raise = SIGNAL_HUP;
				break;
			case SIGTERM:
				loge(LOG_DEBUG, "Received SIGTERM");
				raise = SIGNAL_TERM;
				break;
			case SIGINT:
				loge(LOG_DEBUG, "Received SIGINT");
				raise = SIGNAL_INT;
				break;
			case SIGPIPE:
				break;
		}

		/*
		 * If we've raised a signal and we don't blame to ignore it,
		 * then we need to set it.
		 */
		if (raise) {
			SigLock();
			RaiseSignal(sigtrack, raise);
			SigUnlock();
		}
	}		

	return NULL;
}

int	signal_if(unsigned int signum, int (*fptr)(), int *r) {
	unsigned int lsig;

	/* Accessing 'sigtrack' should be atomic */
	SigLock();

	/* Set sigtrack to something local */
	lsig = sigtrack;

	/* Allow the signal to change */
	SigUnlock();

	/* Test whether ot not we have even a signal */
	if (lsig == 0) 
		return 0;

	/* Can only be a single signal */
	if (!sigtrack_unique_bits(signum)) 
		return -1;

	/* If it happens to match, then we have a winner */
	if (lsig & signum) {
		/* First, we lower the signal.  If we have fptr set to NULL, then we're basically ignoring */
		SigLock();

		/* lower it */
		LowerSignal(sigtrack, signum);

		/* Unlock atomic operation */
		SigUnlock();
	
		/* Cannot be null */
		if (fptr) {
			int tr = 0;
			
			/* Call the function */
			tr = fptr(signum);

			/* Set ``r'' if that's our destiny */
			if (r)
				*r = tr;
		}
		return 0;
	}
	return 1;
}

static int sigtrack_unique_bits(unsigned int signum) {
	int	i, bits = 0;

	for (i = 0; i < 31; i++) {
		if (((signum >> i) & 1)) {
			if (bits++) {
				loge(LOG_DEBUG, "sigtrack_unique_bits: Too many bits were set.");
				return 0;
			}
		}
	}
	return 1;
}

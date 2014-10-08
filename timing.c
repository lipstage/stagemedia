#include "stagemedia.h"

int	trans_buf_min(int min_seconds, int channels, int bitrate, int el_size) {
	return ( bitrate * el_size * channels * min_seconds );
}

int	trans_quot(int delta, int channels, int bitrate, int el_size) {
	int	mysize, tsize;
	float	percent;

	//return 15000;

	/* Size in 1 second */
	mysize = bitrate * el_size * channels;

	/* calculate the multiplier value */
	percent = (((float) delta) / ((float) 1000));

	/* get the actual size we're looking for */
	tsize = percent * mysize;

	/* We must be on the same boundary as the element size */	
	while ( (tsize > 0) && ( (tsize % el_size) != 0))
		tsize++;

	return tsize;
}

int	ms_diff(MSDiff *diff) {
	struct	timespec	tv;
	unsigned long long int	ts_cur;
	int	ret;
	float	temp;

	/* Get the "raw" time, and avoid problems with gettimeofday() */
	clock_gettime(CLOCK_MONOTONIC_RAW, &tv);

	ts_cur = ( (unsigned long long int)tv.tv_sec) * 1000;
	temp = roundf (((float)tv.tv_nsec) / ((float)1000000));
	ts_cur += (unsigned long long int) temp;
	
	if (!diff->ts_last) {
		diff->ts_last = ts_cur;
		return 0;
	}

	ret = (int)(ts_cur - diff->ts_last);

	if (ret < diff->mindiff)
		return 0;

	//printf("TV: %lu %lu | %f %f\n", tv.tv_sec, tv.tv_nsec, diff->ts_last, ts_cur);
	
	if (ret < 0) {
		printf("Something done blown up!\n");
		printf("%d\n", ret);
		//printf("%f\n", diff->ts_last);
		//printf("%f\n", ts_cur);
		return 0;
	}

	diff->ts_last = ts_cur;
	return ( ret > 0 ? ret : 0 );
}

void	ms_diff_init(MSDiff *diff) {
	diff->ts_last = 0;
	diff->mindiff = 0;
	ms_diff(diff);
}

void	ms_diff_set_difference(MSDiff *diff, int value) {
	diff->mindiff = value;
}

#ifndef	__TIMING_H__
#define	__TIMING_H__

typedef	struct MSDiff {
//	float	ts_last;
	int	mindiff;				/* the minimum difference required, or else 0 is returned */
	unsigned long long int	ts_last;
} MSDiff;

int	ms_diff(MSDiff *);
int	trans_buf_min(int, int, int, int);
int	trans_quot(int, int, int, int);
void	ms_diff_init(MSDiff *);
void	ms_diff_set_difference(MSDiff *, int);

#endif

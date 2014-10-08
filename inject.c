#include "stagemedia.h"

int	inject_advertising(pThreads p) {
	FILE	*fp = fopen("/var/www/html/files/blah.pcm", "r");
	char	buffer[1024];
	int	count = 0;	

	atomic();
	bytes_type_set(p->rbuf, BYTES_TYPE_DEFAULT);
	SetAdvertising(p);
	noatomic();

	for (count = 0; count < 44100*2; count++) {
		short int x = 0;

		atomic();
		p->rbuf = bytes_append(p->rbuf, &x, 2);
		noatomic();
	}

	while ((count = fread(buffer, 1, sizeof buffer, fp)) > 0) {
		atomic();
		p->rbuf = bytes_append(p->rbuf, buffer, count);	
		noatomic();
	}
	atomic();
	bytes_type_set(p->rbuf, BYTES_TYPE_RING);
	UnSetAdvertising(p);
	noatomic();

	fclose(fp);

	return 0;
}

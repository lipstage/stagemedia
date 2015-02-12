#include "stagemedia.h"

typedef struct st_URLData {
	char	*data;
	size_t	s;
} URLData;

int	api_receive();
int	valid_session(char *);

/*
 * Fetch a request in raw format
 */
char	*url_fetch(const char *url, const char *post) {
	CURL	*curl;
	CURLcode	res;
	URLData		urld;
	const	char	*system_key;
	struct  curl_slist *chunk = NULL;

	urld.data = NULL;
	urld.s = 0;

	curl = curl_easy_init();
	
	if (!curl)
		return 0;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, api_receive);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &urld);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

	loge(LOG_DEBUG, 
		"url_fetch: Attempting remote query.  url=\"%s\"", url);

	if (post)
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
	
	/* The API system may want an actual cookie -- which is a permissions deal */
	if ((system_key = cfg_read_key("system_key"))) {
		char	cookie[1024];
		
		snprintf(cookie, sizeof cookie - 1, "Cookie: system-session-id=%s", system_key);
		chunk = curl_slist_append(chunk, cookie);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	}
	
	res = curl_easy_perform(curl);

	if (res != CURLE_OK)
		loge(LOG_DEBUG, "url_fetch: Remote server did not return OK");
	else
		loge(LOG_DEBUG, "url_fetch: Remote server returned OK");

	curl_easy_cleanup(curl);

	if (chunk)
		curl_slist_free_all(chunk);

	return urld.data;
}

int	api_receive(void *ptr, size_t size, size_t num, URLData *urld) {
	int	mysize = size * num;
	char	*temp;

	temp = realloc(urld->data, urld->s + mysize + 1);
	if (temp) {
		urld->data = temp;
		memcpy(&urld->data[urld->s], ptr, mysize);
		urld->s += mysize;
		urld->data[urld->s] = '\0';
	}

	return (size * num);
}

/*
 * Make a remote call to see if a sessionid is valid
 */
int	valid_session(char *sessionid) {
	int	isok = -1;
	json_t	*root, *ret;
	json_error_t	error;
	char	*text;
	char	url[4096];
	
	/* 
	 * Moved to a configuration option for the client.
	 *
	 * snprintf(url, sizeof url - 1, "%s/%s/%s", API_SYSTEM_URL, API_LOGGED_SESSION, sessionid); 
 	 */
	snprintf(url, sizeof url - 1, "%s/%s/%s", cfg_read_key("api_system_url"), cfg_read_key("api_logged_session"), sessionid);
	if (!(text = url_fetch(url, NULL)))
		return -1;

	if ((root = json_loads(text, 0, &error)) && json_is_object(root)) {			
		ret = json_object_get(root, "ret");
		isok = json_is_boolean(ret) ? json_is_true(ret) : -1;
		json_decref(root);
	} 
	free(text);
	
	return (isok);
}


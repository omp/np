#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <json.h>
#include <curl/curl.h>

#define USER "user"
#define KEY "key"

static CURL *curl;

struct track {
	char *artist, *album, *title, *user_play_count, *top_tag;
};

void track_free(struct track *t)
{
	free(t->artist);
	free(t->album);
	free(t->title);
	free(t->user_play_count);
	free(t->top_tag);
	free(t);
}

struct json_builder {
	json_object *obj;
	json_tokener *tok;
};

size_t parse_cb(char *ptr, size_t size, size_t nmemb, struct json_builder *jb)
{
	jb->obj = json_tokener_parse_ex(jb->tok, ptr, nmemb);
	return nmemb;
}

json_object *perform_json_curl(char *uri)
{
	struct json_builder jb = {0, json_tokener_new()};

	curl_easy_setopt(curl, CURLOPT_URL, uri);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, parse_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &jb);
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) exit(1);

	json_tokener_free(jb.tok);
	return jb.obj;
}

json_object *fetch_recent_tracks(const char *user)
{
	char uri[2048];
	snprintf(
		uri, 2048,
		"http://ws.audioscrobbler.com/2.0/?method=user.getrecenttracks&"
		"api_key="KEY"&user=%s&format=json",
		user);

	return perform_json_curl(uri);
}

json_object *fetch_track_info(const char *user, char *artist, char *title)
{
	char *c_artist = curl_easy_escape(curl, artist, 0);
	char *c_title = curl_easy_escape(curl, title, 0);

	char uri[2048];
	snprintf(
		uri, 2048,
		"http://ws.audioscrobbler.com/2.0/?method=track.getInfo&"
		"api_key="KEY"&user=%s&artist=%s&track=%s&format=json",
		user, c_artist, c_title);

	curl_free(c_artist);
	curl_free(c_title);

	return perform_json_curl(uri);
}

const char *get_artist(json_object *obj)
{
	json_object *tmp;
	json_object_object_get_ex(obj, "artist", &tmp);
	json_object_object_get_ex(tmp, "#text", &tmp);
	return json_object_get_string(tmp);
}

const char *get_album(json_object *obj)
{
	json_object *tmp;
	json_object_object_get_ex(obj, "album", &tmp);
	json_object_object_get_ex(tmp, "#text", &tmp);
	return json_object_get_string(tmp);
}

const char *get_title(json_object *obj)
{
	json_object *tmp;
	json_object_object_get_ex(obj, "name", &tmp);
	return json_object_get_string(tmp);
}

struct track *get_recent_track(const char *user)
{
	json_object *tmp, *json = fetch_recent_tracks(user);
	json_object_object_get_ex(json, "recenttracks", &tmp);
	json_object_object_get_ex(tmp, "track", &tmp);
	tmp = json_object_array_get_idx(tmp, 0);

	struct track *t = calloc(1, sizeof(*t));
	const char *str;
	if ((str = get_artist(tmp)))
		t->artist = strdup(str);
	if ((str = get_album(tmp)))
		t->album = strdup(str);
	if ((str = get_title(tmp)))
		t->title = strdup(str);

	json_object_put(json);
	return t;
}

const char *get_user_play_count(json_object *obj)
{
	json_object *tmp;
	json_object_object_get_ex(obj, "userplaycount", &tmp);
	return json_object_get_string(tmp);
}

const char *get_top_tag(json_object *obj)
{
	json_object *tmp;
	json_object_object_get_ex(obj, "toptags", &tmp);
	json_object_object_get_ex(tmp, "tag", &tmp);
	if (json_object_get_type(tmp) == json_type_array)
		tmp = json_object_array_get_idx(tmp, 0);
	json_object_object_get_ex(tmp, "name", &tmp);
	return json_object_get_string(tmp);
}

void add_track_info(struct track *t, const char *user)
{
	if (!t->artist || !t->title) return;

	json_object *tmp, *json = fetch_track_info(user, t->artist, t->title);
	json_object_object_get_ex(json, "track", &tmp);

	const char *str;
	if ((str = get_user_play_count(tmp)))
		t->user_play_count = strdup(str);
	if ((str = get_top_tag(tmp)))
		t->top_tag = strdup(str);

	json_object_put(json);
}

int main(int argc, char *argv[])
{
	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();
	if (!curl) exit(1);

	const char *user = (argc > 1) ? argv[1] : USER;
	struct track *t = get_recent_track(user);
	add_track_info(t, user);

	printf("np");
	if (argc > 1)
		printf(" (%s)", user);
	printf(": %s - ", t->artist);
	if (t->album && strlen(t->album) > 0)
		printf("%s - ", t->album);
	printf("%s (plays: %s)", t->title, t->user_play_count ?: "0");
	if (t->top_tag)
		printf(" (%s)", t->top_tag);
	printf("\n");

	track_free(t);
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	return 0;
}

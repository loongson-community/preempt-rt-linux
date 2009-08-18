#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <locale.h>
#include <X11/Xlib.h>
#include <sys/time.h>
#include <signal.h>
#include <linux/ioctl.h>
#include <glib.h>
#include <pthread.h>
#include "xosd.h"
#include "version.h"

#define OSD_TIMEOUT 3

#define PID_FILE "/etc/fnkey/fnkey.pid"
#define EVENT_FILE "/proc/sci"
#define CALL_BACK "/etc/fnkey/default.sh"
#define CONF_FILE "/etc/fnkey/fnkey.conf"

#define VOL_LEVEL 10
#define BRIGHT_LEVEL 8

#define SUSPEND 0x25
#define TOGGLE_BACK_LIGHT 0x2b
#define SWITCH_DISPLAY_MODE 0x24
#define AUDIO_MUTE 0x2c
#define TOGGLE_WIFI 0x30
#define AUDIO_VOLUME 0x2f
#define BRIGHTNESS 0x2d
#define AC 0x2e
#define CRT 0x27
#define CAMERA 0x28
#define LID 0x23

#define oops(msg) do { fprintf(stderr, "line %d: ", __LINE__); perror(msg); } while (0);

typedef struct {
	int key;		/* index of keys */
	int action;		/* index of actions in key */
} event_t;

typedef struct {
	int key;
	char *name;
	char *action[3];
} fnkey_t;

GHashTable *msg;
GArray *key_mask;
xosd *osd;
char *default_font;
event_t event;
int level;			/* volume, brightness */

fnkey_t keys[] = {
	{CAMERA, "camera", "off", "on", NULL},
	{SUSPEND, "suspend", NULL, NULL, NULL},
	{TOGGLE_BACK_LIGHT, "screen", "off", "on", NULL},
	{SWITCH_DISPLAY_MODE, "switchvideo", "lcd", "crt", "both"},
	{AUDIO_MUTE, "mute", "off", "on", NULL},
	{AUDIO_VOLUME, "volume", NULL, NULL, NULL},
	{AC, "ac", "off", "on", NULL},
	{BRIGHTNESS, "brightness", NULL, NULL, NULL},
	{TOGGLE_WIFI, "wifi", "off", "on", NULL},
	{CRT, "crt", "off", "on", NULL},
	{LID, "lid", "off", "on", NULL},
	{-1, NULL,},
};

#define THREAD_NUM 3
pthread_t thread[THREAD_NUM];
void *thread_func[THREAD_NUM];
int event_is_ready, display_is_ready, action_is_ready;
#define SLEEP_LEN 100000

void crash_handler()
{
	int pid;
	char buf[11];

	pid = getpid();
	if (fork() == 0) {
		fprintf(stderr, "****************START*************\n");
		bzero(buf, 11);
		sprintf(buf, "%d", pid);
		if (execlp
		    ("gdb", "gdb", "-c", buf, "-batch", "-command=gdb-cmd",
		     NULL) < 0) {
			oops("execlp");
			exit(0);
		}
	} else {
		wait(0);
		fprintf(stderr, "*****************END**************\n");
		exit(0);
	}
}

int get_index_of_keys(int raw_key)
{
	int i;

	for (i = 0; keys[i].key != -1; i++)
		if (keys[i].key == raw_key)
			return i;

	return -1;
}

void *do_action(void)
{
	char cmd[1024];
	int i, j;

	do {
		while (!event_is_ready || action_is_ready)
			usleep(SLEEP_LEN);
		action_is_ready = 1;

		i = event.key;
		j = event.action;
		if (keys[i].key == AUDIO_VOLUME || keys[i].key == BRIGHTNESS)
			sprintf(cmd, "%s %s %d &", CALL_BACK, keys[i].name,
				level);
		else
			sprintf(cmd, "%s %s %s &", CALL_BACK, keys[i].name,
				keys[i].action[j]);
		system(cmd);
	} while (1);

	pthread_exit(0);
}

event_t *get_event(int fd)
{
#define SCI_EVENT_LEN 15	/* /proc/sci: 0x2d    5 */
	char buf[SCI_EVENT_LEN];
	int i, raw_key, raw_action;
	unsigned short vga = 0;
	unsigned short toggle_display_state = 0;

	bzero(buf, SCI_EVENT_LEN);
	if (read(fd, buf, SCI_EVENT_LEN) < 0) {
		oops("read from fd");
		return NULL;
	}
#ifdef DEBUG
	fprintf(stderr, "read from sci: %s\n", buf);
#endif

	sscanf(buf, "%x\t%d\n", &raw_key, &raw_action);

#ifdef DEBUG
	printf("raw key: %x, raw action: %d\n", raw_key, raw_action);
#endif

	/* get key(index of keys) */
	i = get_index_of_keys(raw_key);
	if (i == -1) {
		oops("No such key");
		return NULL;
	} else
		event.key = i;

	/* get action(index of actions in key) */
	switch (raw_key) {
	case SWITCH_DISPLAY_MODE:
		toggle_display_state =
		    vga ? ((toggle_display_state + 1) % 3) : 0;
		raw_action = toggle_display_state;
		break;
	case AC:
		if (raw_action == 3)
			return NULL;
		break;
	case AUDIO_VOLUME:
	case BRIGHTNESS:
		if (raw_key == AUDIO_VOLUME)
			raw_action = raw_action * 100 / VOL_LEVEL;
		else
			raw_action = raw_action * 100 / BRIGHT_LEVEL + 10;
		level = raw_action;
		raw_action = 0;
		break;
 deafult:
		return NULL;
	}
	event.action = raw_action;

#ifdef DEBUG
	printf("key: %d, action: %d\n", event.key, event.action);
#endif
	return &event;
}

void *poll_event(void)
{
	int fd, len;
	struct pollfd pfd;

	fd = open(EVENT_FILE, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		oops("open");
		return;
	}

	fcntl(fd, F_SETFD, FD_CLOEXEC);
	pfd.fd = fd;
	pfd.events = POLLIN;

	do {
		int r;

		r = poll(&pfd, 1, -1);
		if (r < 0) {
			oops("poll");
			continue;
		}

		if (pfd.revents == POLLIN) {
			event_t *p;
			p = get_event(fd);
			if (p) {
				event_is_ready = 1;

				while (!display_is_ready || !action_is_ready)
					usleep(SLEEP_LEN);
				display_is_ready = 0;
				action_is_ready = 0;
				event_is_ready = 0;
			}
		}
	} while (1);

	pthread_exit(0);
}

void iterator(gpointer key, gpointer value, gpointer user_data)
{
	g_printf(user_data, key, value);
}

void *on_screen_display(void)
{
	char event_string[100];
	int i, j;
	gchar *p;

	if (setlocale(LC_ALL, "") == NULL || !XSupportsLocale())
		fprintf(stderr,
			"Locale not available, expect problems with fonts.\n");

	osd = xosd_create(1);

	if (0 != xosd_set_font(osd, (char *)osd_default_font))
		oops("xosd_set_font");

	if (0 != xosd_set_timeout(osd, OSD_TIMEOUT))
		oops("xosd_set_timeout");

	xosd_set_pos(osd, XOSD_middle);
	xosd_set_align(osd, XOSD_center);
	xosd_set_bar_height(osd, 80);
	xosd_set_font(osd, default_font);

	bzero(event_string, 50);

	do {
		while (!event_is_ready || display_is_ready)
			usleep(SLEEP_LEN);
		display_is_ready = 1;

		i = event.key;
		j = event.action;
#ifdef DEBUG
		printf("key:%d, action: %s\n", keys[i].name, keys[i].action[j]);
#endif
		switch (keys[i].key) {
		case AUDIO_VOLUME:
			xosd_set_bar_length(osd, VOL_LEVEL);
			xosd_set_bar_icon(osd, XOSD_volume);
			break;
		case BRIGHTNESS:
			xosd_set_bar_length(osd, BRIGHT_LEVEL);
			xosd_set_bar_icon(osd, XOSD_brightness);
			break;
		case AUDIO_MUTE:
		case SUSPEND:
		case TOGGLE_BACK_LIGHT:
		case SWITCH_DISPLAY_MODE:
		case CRT:
		case CAMERA:
		case TOGGLE_WIFI:
		case AC:
			if (keys[i].key == SUSPEND)
				sprintf(event_string, "%s", keys[i].name);
			else
				sprintf(event_string, "%s.%s", keys[i].name,
					keys[i].action[j]);
#ifdef DEBUG
			printf("get_event, event_string: %s\n", event_string);
			g_hash_table_foreach(msg, (GHFunc) iterator,
					     "key: %s value: %s\n");
#endif
			p = g_hash_table_lookup(msg, event_string);
			if (p == NULL)
				sprintf(event_string, "%s %s", keys[i].name,
					keys[i].action[j]);
			else
				sprintf(event_string, "%s", p);
		default:
			xosd_set_bar_icon(osd, XOSD_null);
		}

		if ((keys[i].key == AUDIO_VOLUME)
		    || (keys[i].key == BRIGHTNESS)) {
#ifdef DEBUG
			printf("Volume/Brightness level: %d\n", level);
#endif
			if (-1 == xosd_display(osd, 0, XOSD_percentage, level))
				oops("xosd_SWITCH_DISPLAY_MODE");
		} else {
#ifdef DEBUG
			printf("event_string: %s\n", event_string);
#endif
			if (-1 ==
			    xosd_display(osd, 0, XOSD_string, event_string))
				oops("xosd_SWITCH_DISPLAY_MODE");
		}
	} while (1);

	if (0 != xosd_wait_until_no_display(osd))
		oops("xosd_wait_until_no_display");

	if (0 != xosd_destroy(osd))
		oops("destroy");

	pthread_exit(0);
}

void get_config(void)
{
#define KEY_NAME_LEN 15
	GKeyFile *config;
	gchar *p, key[KEY_NAME_LEN];
	int i, j;

	config = g_key_file_new();
	g_key_file_load_from_file(config, CONF_FILE, G_KEY_FILE_NONE, NULL);

	/* font */
	default_font = g_key_file_get_value(config, "general", "font", NULL);
	if (default_font == NULL)
		default_font = strdup("Sans-24");

	/* event mask */
	key_mask = g_array_new(FALSE, TRUE, sizeof(gint));
	for (i = 0; keys[i].key != -1; i++) {
		p = g_key_file_get_value(config, "mask", keys[i].name, NULL);
		if (p && strcmp("enable", p))
			g_array_append_val(key_mask, keys[i].key);
	}

	/* built event message hash table */
	msg = g_hash_table_new(g_str_hash, g_str_equal);
	for (i = 0; keys[i].key != -1; i++) {
		for (j = 0; j < 3; j++) {
			bzero(key, KEY_NAME_LEN);
			if (keys[i].action[j])
				sprintf(key, "%s.%s", keys[i].name,
					keys[i].action[j]);
			else {
				if (j != 0)
					continue;
				else
					sprintf(key, "%s", keys[i].name);
			}

			if (g_key_file_has_key(config, "message", key, NULL)) {
				p = g_key_file_get_string(config, "general",
							  "locale", NULL);
				if (p) {
					p = g_key_file_get_locale_string(config,
									 "message",
									 key,
									 p,
									 NULL);
					if (p)
						/* strdup(key) must be used instead of key, for glib hash
						 * table need an unique address for any key */
						g_hash_table_insert(msg,
								    strdup(key),
								    p);
				} else {
					p = g_key_file_get_string(config,
								  "message",
								  key, NULL);
					if (p)
						g_hash_table_insert(msg,
								    strdup(key),
								    p);
				}
			}
		}
	}

	return;
}

void show_version(void)
{
	printf("YeeLoong Netbook function key utils, version %s\n", VERSION);

	return;
}

void thread_create(void)
{
	int i, tmp;

	bzero(&thread, THREAD_NUM);
	for (i = 0; i < THREAD_NUM; i++) {
		tmp = pthread_create(&thread[i], NULL, thread_func[i], NULL);
		if (tmp != 0)
			oops("pthread_create thread1\n");
	}
	return;
}

void thread_wait(void)
{
	int i;

	for (i = 0; i < THREAD_NUM; i++)
		if (thread[i] != 0)
			pthread_join(thread[i], NULL);
}

int daemon_init(void)
{
	pid_t pid;

	pid = fork();
	if (pid < 0)
		return -1;
	else if (pid != 0)
		exit(0);
	setsid();
	chdir("/");
	umask(0);

	return 0;
}

/* ensure there is only one copy of me running */
void set_one_copy(void)
{
	int fd, val;
	char buf[10];
	struct flock lock;

	fd = open(PID_FILE, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd < 0) {
		oops("open error");
		exit(-1);
	}

	lock.l_type = F_WRLCK;
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_len = 0;

	if (fcntl(fd, F_SETLK, &lock) < 0) {
		if (errno == EACCES || errno == EAGAIN) {
			printf("the process is already running\n");
			exit(0);
		} else
			oops("write_lock");
	}
	if (ftruncate(fd, 0) < 0)
		oops("ftruncate");
	sprintf(buf, "%d\n", getpid());
	if (write(fd, buf, strlen(buf)) != strlen(buf))
		oops("write");

	val = fcntl(fd, F_GETFD, 0);
	if (val < 0)
		oops("fcntl F_GETFD");
	val |= FD_CLOEXEC;
	if (fcntl(fd, F_SETFD, val) < 0)
		oops("fcntl F_SETFD");
}

int main(int argc, char *argv[])
{
	/* start fnkey as a daemon process */
	daemon_init();

	/* ensure only one copy of me running */
	set_one_copy();

	/* the version information is defined in VERSION */
	show_version();

	/* the default config file is CONF_FILE */
	get_config();

	/* register a handler for segmentation fault for error diagnosis */
	signal(SIGSEGV, crash_handler);

	event_is_ready = 0;
	action_is_ready = 0;
	display_is_ready = 0;

	thread_func[0] = do_action;
	thread_func[1] = on_screen_display;
	thread_func[2] = poll_event;

	thread_create();
	thread_wait();

	return 0;
}

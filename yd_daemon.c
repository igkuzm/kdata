/**
 * File              : yd_daemon.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 20.07.2022
 * Last Modified Date: 27.09.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>  //for sleep

#include "kdata.h"
#include "SQLiteConnect/SQLiteConnect.h"
#include "cYandexDisk/cYandexDisk.h"


#define NEW(T) ({T *new = malloc(sizeof(T)); new;})
#define STR(...) ({char str[BUFSIZ]; sprintf(str, __VA_ARGS__); str;})

void *
yd_thread(void * data) 
{
	struct yd_data_t *d = data; 

	while (1) {
		if (d->callback)
			if (d->callback(d->user_data, d->thread, "yd_daemon: updating data..."))
				break;
		yd_update(d);
		sleep(SEC);
	}

	free(d);

	pthread_exit(0);	
}

void
yd_daemon_init(
			const char * database_path,
			const char * token,
			kdata_s * s, 
			void * user_data,
			int (*callback)(void * user_data, pthread_t thread, char * msg)
		)
{
	int err;
	pthread_t tid; //thread id
	pthread_attr_t attr; //thread attributives
	
	err = pthread_attr_init(&attr);
	if (err) {
		if (callback)
			callback(user_data, 0, STR("yd_daemon: can't set thread attributes: %d", err));
		exit(err);
	}	

	//set params
	struct yd_data_t *d = NEW(struct yd_data_t);
	strcpy(d->database_path, database_path);
	strcpy(d->token, token);
	d->structure = s;
	d->callback = callback;
	d->thread = tid;
	d->user_data = user_data;
	
	//create new thread
	err = pthread_create(&tid, &attr, yd_thread, d);
	if (err) {
		if (callback)
			callback(user_data, 0, STR("yd_daemon: can't create thread: %d", err));
		exit(err);
	}
}

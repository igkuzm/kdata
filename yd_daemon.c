/**
 * File              : yd_daemon.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 20.07.2022
 * Last Modified Date: 19.09.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>  //for sleep

#include "kdata.h"
#include "yd.h"
#include "SQLiteConnect/SQLiteConnect.h"
#include "cYandexDisk/cYandexDisk.h"


#define STR(...) ({char ___str[BUFSIZ]; sprintf(___str, __VA_ARGS__); ___str;})

// memory allocation helpers
#define MALLOC(size) ({void* const ___p = malloc(size); if(!___p) {perror("Malloc"); exit(EXIT_FAILURE);} ___p;})
#define REALLOC(ptr, size)	({void* const ___s = ptr; void* const ___p = realloc(___s, size); if(!___p) { perror("Realloc"); exit(EXIT_FAILURE); } ___p;})
#define FREE(ptr) ({if (ptr) free(ptr); ptr = NULL;})
#define NEW(T) ((T*)MALLOC(sizeof(T)))


struct yd_data_t{
	char database_path[BUFSIZ];
	char token[128];
	kdata_s * s; 
	void * user_data;
	pthread_t thread;
	int (*callback)(void * user_data, pthread_t thread, char * msg);
	char uuid[37];
	char tablename[256];
	time_t timestamp;
	int deleted;	
};

struct yd_t {
	int id;
	char uuid[37];
	char tablename[256];
	time_t timestamp;
	int localchange;
	int deleted;
};

struct yd_t_array{
	struct yd_t * data;
	int len;
};

struct yd_t_array * yd_t_array_new(){
	struct yd_t_array * array = NEW(struct yd_t_array);
	array->data = NEW(struct yd_t);
	array->len = 0;
	return array;
}

void yd_t_array_append(struct yd_t_array * array, struct yd_t item){
	struct yd_t * data = array->data;
	data[array->len] = item;
	array->len++;
	array->data = REALLOC(array->data, sizeof(struct yd_t) + sizeof(struct yd_t) * array->len);
}

void kdatad_t_array_free(struct yd_t_array * array){
	FREE(array->data);
	FREE(array);
}

#define kdatad_t_array_for_each(array, item)\
		struct yd_t * ___data = array->data;\
		struct yd_t * ___p, item;\
		for (___p = (___data), (item) = *___p; ___p < &((___data)[array->len]); ___p++, (item) = *___p)\

int ya_t_get(void *user_data, int argc, char *argv[], char *titles[])
{
	struct yd_t_array *array = user_data;
	struct yd_t item;

	for (int i = 0; i < argc; ++i) {
		char buff[128];
		if (!argv[i]) buff[0] = '\0'; //no seg falt on null
		else {
			strncpy(buff, argv[i], 127);
			buff[127] = '\0';
		}

		switch (i) {
			case 0:  strcpy(item.uuid, buff)             ; break;
			case 1:  strcpy(item.tablename, buff)        ; break;
			case 2:  item.timestamp = atol(buff)         ; break;
			case 3:  item.localchange = atoi(buff)       ; break;
			case 4:  item.deleted = atoi(buff)           ; break;

			default:                                       break;
		}
	}

	//add item to array
	yd_t_array_append(array, item);	

	return 0;
}

int transfer_callback(size_t size, void *user_data, char *error){
	printf("kdata daemon: starting transfer_callback\n");
	struct yd_data_t *data = user_data;
	if (data == NULL) {
		printf("kdata daemon: ERROR! Data is NULL\n");
		return 1;
	}
	if (error)
		if(data->callback)
			data->callback(data->user_data, data->thread, error);

	//update timestaps table
	if (size) {
		if(data->callback)
			data->callback(data->user_data, data->thread, STR("kdata daemon: updated data for table: %s"
															  ", uuid: %s, timestamp: %ld"
															  ", size: %ld", data->tablename, data->uuid, data->timestamp, size));
		char SQL[BUFSIZ];
		sprintf(SQL, "UPDATE kdata_updates SET localchange = 0, timestamp = %ld WHERE uuid = '%s'", data->timestamp, data->uuid);
		sqlite_connect_execute(SQL, data->database_path);
	}

	return 1;
}

struct uuid_list {
	char uuid[37];
	struct uuid_list * prev;
};

void uuid_list_add(struct uuid_list ** list, const char * uuid) {
	struct uuid_list *new = NEW(struct uuid_list);
	new->prev = *list;
	strncpy(new->uuid, uuid, 36); new->uuid[36] = 0;
	*list = new;
} 

int get_list_of_uuid(c_yd_file_t *file, void *user_data, char *error){
	struct uuid_list ** list = user_data;
	if (error)
		perror(error);

	if (file)
		uuid_list_add(list, file->name);

	return 0;
}

void update_from_cloud_with_data(
			const char * database_path,
			const char * YD_token,
			void * user_data,
			pthread_t thread,
			int (*callback)(void * user_data, pthread_t thread, char * msg),
			const char * uuid,
			const char * tablename,
			time_t timestamp,
			bool deleted
		)
{
		struct yd_data_t data;
		{
			strcpy(data.database_path, database_path);
			strcpy(data.token, YD_token);
			data.user_data = user_data;
			data.thread = thread;
			data.callback = callback;
			strcpy(data.uuid, uuid);
			strcpy(data.tablename, tablename);
			data.timestamp = timestamp;
			data.deleted = deleted;
		}
		//update
		printf("kdata daemon: try to update from cloud: TOKEN: %s; PATH: %s; DB: %s; TABLE: %s; UUID: %s; TIME: %ld\n", YD_token, PATH, database_path, tablename, uuid, timestamp);
		sqlite2yandexdisk_update_from_cloud(
			YD_token, 
			PATH, 
			database_path, 
			tablename, 
			uuid, 
			timestamp, 
			false, 
			&data, //void *user_data, 
			transfer_callback //int (*callback)(size_t, void *, char *)
		);	
}

void yd_update_data(struct yd_data_t * d)
{
	int i, k;

	//get list of updates 
	struct yd_t_array * array = yd_t_array_new();	
	char SQL[] = "SELECT * FROM kdata_updates";
	sqlite_connect_execute_function(SQL, d->database_path, array, ya_t_get);

	//update from cloud
	char * tables[] = {"rating", "images"};
	for (k = 0; k < 1; ++k) {
		struct uuid_list *list = NEW(struct uuid_list); list->prev = NULL;
		char path[BUFSIZ];
		sprintf(path, "app:/%s/%s", PATH, tables[k]);

		//get list of uuid
		c_yandex_disk_ls(
			d->token, 
			path, 
			&list, 
			get_list_of_uuid
		);

		//for each uuid check timstamp and update if needed
		while(list->prev != NULL) {
			char *uuid = list->uuid;
			bool isNew = true;
			kdatad_t_array_for_each(array, t) {
				if (strcmp(uuid, t.uuid) == 0) {
					isNew = false;
					if (!t.deleted){
						update_from_cloud_with_data(
							d->database_path,
							d->token,
							d->user_data,
							d->thread,
							d->callback,
							t.uuid,
							tables[k],
							t.timestamp,
							t.deleted
						);
					}
				}
			}
			//create new one
			if (isNew) {
				update_from_cloud_with_data(
					d->database_path,
					d->token,
					d->user_data,
					d->thread,
					d->callback,
					uuid,
					tables[k],
					0,
					0
				);
			}
			//free memory & cicle
			struct uuid_list *ptr = list;
			list = list->prev;
			free(ptr);
		}
		free(list);
	}

	//upload to cloud
	//for each timestamp
	kdatad_t_array_for_each(array, t) {
		if (t.localchange) {
			if (t.deleted) {
				//delete from yandex disk
				printf("yd daemon: try to delete: TOKEN: %s; PATH: %s; DB: %s; TABLE: %s; UUID: %s\n", d->token, PATH, d->database_path, t.tablename, t.uuid);
				char * error = NULL;
				char path[BUFSIZ];
				sprintf(path, "app:/%s/%s/%s", PATH, t.tablename, t.uuid);						
				c_yandex_disk_rm(d->token, path, &error);
				if (error){
					if (d->callback)
						d->callback(0, d->thread, error);
				}else {
					//update updates table
					char SQL[BUFSIZ];
					sprintf(SQL, "UPDATE kdata_updates SET localchange = 0, timestamp = %ld WHERE uuid = '%s'", d->timestamp, d->uuid);
					sqlite_connect_execute(SQL, d->database_path);				
				}
			} else {
				//update to Yandex Disk
				struct yd_data_t * data = NEW(struct yd_data_t);
				{
					strcpy(data->database_path, d->database_path);
					strcpy(data->token, d->token);
					data->user_data = d->user_data;
					data->thread = d->thread;
					data->callback = d->callback;
					strcpy(data->uuid, t.uuid);
					strcpy(data->tablename, t.tablename);
					data->timestamp = t.timestamp;
					data->deleted = t.deleted;
				}

				printf("yd daemon: try to upload: TOKEN: %s; PATH: %s; DB: %s; TABLE: %s; UUID: %s; TIME: %ld\n", d->token, PATH, d->database_path, t.tablename, t.uuid, t.timestamp);
				sqlite2yandexdisk_upload(
					d->token, 
					PATH, 
					d->database_path, 
					t.tablename, 
					t.uuid, 
					t.timestamp, 
					data, //void *user_data, 
					transfer_callback //int (*callback)(size_t, void *, char *)
				);
			}
		}
	}

	//free memory
	kdatad_t_array_free(array);
}

void *
yd_thread(void * data) 
{
	struct yd_data_t *d = data; 

	while (1) {
		if (d->callback)
			if (d->callback(d->user_data, d->thread, "kdata daemon: updating data..."))
				break;
		yd_update_data(d);
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
			callback(user_data, 0, STR("kdata daemon: can't set thread attributes: %d", err));
		exit(err);
	}	

	//set params
	struct yd_data_t *d = NEW(struct yd_data_t);
	strcpy(d->database_path, database_path);
	strcpy(d->token, token);
	d->callback = callback;
	d->thread = tid;
	d->user_data = user_data;
	
	//create new thread
	err = pthread_create(&tid, &attr, yd_thread, d);
	if (err) {
		if (callback)
			callback(user_data, 0, STR("kdata daemon: can't create thread: %d", err));
		exit(err);
	}
}

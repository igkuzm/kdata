/* File              : yd_update.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 20.09.2022
 * Last Modified Date: 21.09.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

/*
 * Implimation of functions to check updates is needed and create lists to update transfers
 */

#include <string.h>

#include "kdata.h"
#include "cYandexDisk/cYandexDisk.h"
#include "SQLiteConnect/SQLiteConnect.h"

/*
 * macros
 */
#define NEW(T) ({T *new = malloc(sizeof(T)); new;})
#define STR(...) ({char str[BUFSIZ]; sprintf(str, __VA_ARGS__); str;})

/*
 * list functions 
 */

typedef struct list_t {
	struct list_t *next;
	void *data;
}list_t;

struct list_t * list_new(){
	struct list_t *list = NEW(struct list_t);
	list->next = NULL;
	list->data = NULL;
	return list;
}

void list_add(struct list_t **list, void *data){
	if (*list){
		struct list_t *new = list_new();
		new->next = *list;
		new->data = data;
		*list = new;
	}
}

void list_remove(struct list_t *list, struct list_t * node){
	while (list) {
		struct list_t *next = list->next;
		if (next == node){
			list->next = next->next;
		}
		//iterate
		list = list->next;
	}
}

void list_free(struct list_t *list){
	while(list->next){
		free(list->data);
		list_t *ptr = list;
		list = list->next;
		free(ptr);
	}
	free(list);
}

#define list_for_each(list, item) \
	struct list_t * ___ptr = list; \
	void* item; \
	for (item = ___ptr->data; ___ptr->next; ___ptr=___ptr->next, item = ___ptr->data)

/*
 * Implimation
 */

struct list_callback_s {
	list_t ** l;
	struct yd_data_t *d;
};

int uuids_in_cloud_callback(c_yd_file_t *file, void * user_data, char * error){
	struct list_callback_s *s = user_data;
	list_t ** l = s->l;
	struct yd_data_t *d = s->d;
	if (error){
		if (d->callback)
			d->callback(d->user_data, d->thread, error);
		return 0;
	}
	if (file){
		char *uuid = malloc(37);
		if (uuid){
			strncpy(uuid, file->name, 36); uuid[36] = 0;
			list_add(l, uuid);
		}
	}

	return 0;
}

int updates_in_database_callback(void *user_data, int argc, char **argv, char **titles){
	struct update_s *s = NEW(struct update_s);
	if (s){
		list_t ** l = user_data;
		int i;
		for (int i = 0; i < argc; i++) {
			char *value = argv[i];
			if (!value) 
				value ="";
			switch (i) {
				case 0:
				   strncpy(s->uuid, value, 36); s->uuid[36] = 0; break;
				case 1:
				   strncpy(s->tablename, value, 127); s->tablename[127] = 0; break;
				case 2:
				   s->timestamp = atol(value); break;
				case 3:
				   s->localchange = atoi(value); break;
				case 4:
				   s->deleted = atoi(value); break;
			}
		}
		list_add(l, s);
	}

	return 0;
}

int timestamps_callback(c_yd_file_t *file, void * user_data, char * error){
	struct list_callback_s *s = user_data;
	list_t ** l = s->l;
	struct yd_data_t *d = s->d;

	if (error){
		if (d->callback)
			d->callback(d->user_data, d->thread, error);
		return 0;
	}
	if (file){
		time_t *timestamp = malloc(sizeof(time_t));
		if (timestamp){
			*timestamp = atol(file->name);
			list_add(l, timestamp);
		}
	}

	return 0;
}

void
yd_update_list_compare(
		struct yd_data_t *d,
		struct list_t **list_to_upload,
		struct list_t **list_to_download,
		list_t * updates_in_database,
		list_t * uuids_in_cloud,
		const char *tablename,
		const char * path,
		bool deleted
		)
{
	//for each uuid in cloud
	list_for_each(uuids_in_cloud, item){
		char *uuid = item;
		bool new_to_download = true; //by default is new to download
		//get timestamps
		list_t * timestamps = list_new();	
		struct list_callback_s cs = {.d = d, .l = &timestamps};
		char datapath[BUFSIZ];
		sprintf(datapath, "app:/%s/%s/%s", path, tablename, uuid);
		int err = c_yandex_disk_ls(d->token, path, &cs, timestamps_callback);
		//find max timestamp
		time_t max = 0;
		while (timestamps->next) {
			time_t *timestamp = timestamps->data;
			if (*timestamp > max)
				max = *timestamp;
			//iterate and free timestamp
			free(timestamp);
			list_t * timestamps_ptr = timestamps;
			timestamps = timestamps->next;
			free(timestamps_ptr);
		}
		//for each in updates
		list_t * u = updates_in_database;
		while (u->next){
			struct update_s *update = u->data; 
			//if uuid matches	
			if (!strcmp(uuid, update->uuid)){
				new_to_download = false; //it's not new
				//compare timestamps
				if ((update->timestamp > max)){
					//keep in list_to_upload
				} else {
					//remove from upload list
					list_remove(*list_to_upload, item);
				} 
				if (update->timestamp < max){
					struct update_s *update = NEW(struct update_s);
					update->timestamp = max;
					update->localchange = false;
					update->deleted = deleted;
					strcpy(update->tablename, tablename);
					strcpy(update->uuid, uuid);
					list_add(list_to_download, update);
				}
			}

			//iterate
			u = u->next;
		}
		//if is new to download
		if (new_to_download){
			struct update_s *update = NEW(struct update_s);
			update->timestamp = max;
			update->localchange = false;
			update->deleted = deleted;			
			strcpy(update->tablename, tablename);
			strcpy(update->uuid, uuid);
			list_add(list_to_download, update);
		}	
	}
}

void yd_update(struct yd_data_t *d)
{
	int err = 0;

	//get list of updates in database
	list_t * updates_in_database = list_new();	
	char SQL[] = "SELECT * FROM kdata_updates";
	err = sqlite_connect_execute_function(SQL, d->database_path, &updates_in_database, updates_in_database_callback);
	if (err){
		if (d->callback)
		   d->callback(d->user_data, d->thread, STR("yd_update: can't SELECT * FROM kdata_updates. Error: %d", err));	
		return;
	}

	//list to upload
	struct list_t *list_to_upload = list_new();
	list_for_each(updates_in_database, item){
		list_add(&list_to_upload, item);		
	}	

	//list to download
	struct list_t *list_to_download = list_new();

	//for each table in data and deleted
	char *paths[] = {"data", "deleted"};
	int i;
	for (int i = 0; i < 2; i++) {
		char *path = paths[i];
		kdata_s * s = d->structure;	
		while (s) {
			kdata_table table = s->table;
			//get path
			char datapath[BUFSIZ];
			sprintf(datapath, "app:/%s/%s", path, table.tablename);
			//get list of uuids in cloud
			list_t * uuids_in_cloud = list_new();	
			struct list_callback_s cs = {.d = d, .l = &uuids_in_cloud};
			err = c_yandex_disk_ls(d->token, datapath, &cs, uuids_in_cloud_callback);
			if (err){
				if (d->callback)
				   d->callback(d->user_data, d->thread, STR("yd_update: c_yandex_disk_ls %s. Error: %d", datapath, err));	
			}
			
			//compare
			yd_update_list_compare(
					d,
					&list_to_upload, 
					&list_to_download,
					updates_in_database,
					uuids_in_cloud,
					table.tablename,
					path,
					i
					);

			//free uuids_in_cloud
			list_free(uuids_in_cloud);
			
			//itarate structure
			s=s->next;
		}
	}

	//upload data
	{
		list_for_each(list_to_upload, item){ //no need to free list (pointers to updates_in_database)
			struct update_s *update = item;			

			//upload data
			yd_upload(d, update);
		}
	}

	//download data
	{
		while(list_to_download->next){ //need to free list
			struct update_s *update = list_to_download->data;			
			
			//download data
			yd_download(d, update);

			//free args and iterate
			free(update);
			list_t *ptr = list_to_download;
			list_to_download = list_to_download->next;
			free(ptr);			
		}
		free(list_to_download);
	}

	//free memory
	list_free(updates_in_database);
}

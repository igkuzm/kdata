/* File              : yd_update.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 20.09.2022
 * Last Modified Date: 23.09.2022
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
	struct list_callback_s *s = user_data;
	list_t ** l = s->l;
	struct yd_data_t *d = s->d;
	
	struct update_s *u = NEW(struct update_s);
	if (s){
		int i;
		for (int i = 0; i < argc; i++) {
			char *value = argv[i];
			if (!value) 
				value ="";
			switch (i) {
				case 0:
				   strncpy(u->uuid, value, 36); u->uuid[36] = 0; break;
				case 1:
				   strncpy(u->tablename, value, 127); u->tablename[127] = 0; break;
				case 2:
				   u->timestamp = atol(value); break;
				case 3:
				   u->localchange = atoi(value); break;
				case 4:
				   u->deleted = atoi(value); break;
			}
		}
		list_add(l, u);
		
		if (d->callback)
			d->callback(d->user_data, d->thread, STR("Check update for: %s, uuid: %s, timestamp: %ld, deleted: %s", u->tablename, u->uuid, u->timestamp, u->deleted?"true":"false"));
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
	list_t *uuids = uuids_in_cloud; 
	while(uuids->next){
		char *uuid = uuids->data;
		bool new_to_download = true; //by default is new to download
		//get timestamps
		list_t * timestamps = list_new();	
		struct list_callback_s cs = {.d = d, .l = &timestamps};
		char datapath[BUFSIZ];
		sprintf(datapath, "app:/%s/%s/%s", path, tablename, uuid);
		int err = c_yandex_disk_ls(d->token, datapath, &cs, timestamps_callback);
		if (err){
			if (d->callback)
				d->callback(d->user_data, d->thread, STR("yd_update: yd_update_list_compare %s. Error: %d", datapath, err));	
			return;
		}
		//find max timestamp
		time_t max = 0;
		while (timestamps->next) {
			time_t *timestamp = timestamps->data;
			if (d->callback)
				d->callback(d->user_data, d->thread, STR("yd_update: check timestamp %ld for %s", *timestamp, uuid));	
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
					list_remove(*list_to_upload, u);
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

		//iterate uuids and free data
		list_t * uuids_ptr = uuids;
		uuids = uuids->next;
		free(uuid);
		free(uuids_ptr);
	}
}

void yd_update(struct yd_data_t *d)
{
	int err = 0;

	//get list of updates in database
	list_t * updates_in_database = list_new();	
	struct list_callback_s cs = {.d = d, .l = &updates_in_database};
	char SQL[] = "SELECT * FROM kdata_updates";
	err = sqlite_connect_execute_function(SQL, d->database_path, &cs, updates_in_database_callback);
	if (err){
		if (d->callback)
		   d->callback(d->user_data, d->thread, STR("yd_update: can't SELECT * FROM kdata_updates. Error: %d", err));	
		return;
	}

	//allocate list to upload and fill with updates_in_database
	list_t *list_to_upload = list_new();
	list_t *ptr = updates_in_database;
	while (ptr->next){
		list_add(&list_to_upload, ptr->data);		
		ptr = ptr->next;
	}

	//list to download
	struct list_t *list_to_download = list_new();

	//for each table in data and deleted
	char *paths[] = {"data", "deleted"};
	int i;
	for (int i = 0; i < 2; i++) {
		char *path = paths[i];
		kdata_s * s = d->structure;	
		while (s->next) {
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

			//itarate structure
			s=s->next;
		}
	}

	//upload data
	{
		while(list_to_upload->next){ //no need to free list nodes (pointers to updates_in_database)
			struct update_s *update = list_to_upload->data;			

			//upload data
			if (d->callback)
				d->callback(d->user_data, d->thread, STR("yd_update: try to upload: %s, uuid: %s, timestamp: %ld, deleted: %s", 
							update->tablename, update->uuid, update->timestamp, update->deleted?"true":"false"));
			yd_upload(d, update);

			//iterate
			list_to_upload = list_to_upload->next;
		}
		//free list_to_upload
		free(list_to_upload);
	}

	//download data
	{
		while(list_to_download->next){ //need to free list
			struct update_s *update = list_to_download->data;			
			
			//download data
			d->callback(d->user_data, d->thread, STR("yd_update: try to download: %s, uuid: %s, timestamp: %ld, deleted: %s", 
						update->tablename, update->uuid, update->timestamp, update->deleted?"true":"false"));
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
	while(updates_in_database->next){
		list_t *ptr = updates_in_database;
		updates_in_database = updates_in_database->next;
		free(ptr->data);	
		free(ptr);	
	}
	free(updates_in_database);
}

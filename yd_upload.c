/**
 * File              : yd_upload.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 03.05.2022
 * Last Modified Date: 23.09.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

#include "kdata.h"
#include <stdio.h>
#include <string.h>

#include "SQLiteConnect/SQLiteConnect.h"
#include "cYandexDisk/cYandexDisk.h"

#define NEW(T) ({T *new = malloc(sizeof(T)); new;})
#define STR(...) ({char str[BUFSIZ]; snprintf(str, BUFSIZ-1, __VA_ARGS__); str[BUFSIZ-1]=0; str;})

int
create_directories(
		struct yd_data_t *d, 
		const char * path
		)
{
	if(d->callback)
		d->callback(d->user_data, d->thread, STR("yd_upload: create directories for path: %s", path));
	
	//buffer for path
	char buf[BUFSIZ];
	strncpy(buf, path, BUFSIZ - 1); buf[BUFSIZ - 1] = '\0';
	//make directory for path - strtok to use path components
	char _path[BUFSIZ];
	char *p = strtok(buf, "/");
	sprintf(_path, "%s", p);
	while (p) {
		char *error = NULL;
		if (strcmp("app:", _path)) //don't create 'app:' path
			c_yandex_disk_mkdir(d->token, _path, &error);
		if (error) {
			if(d->callback)
				d->callback(d->user_data, d->thread, STR("yd_upload: can't create directory %s: %s", _path, error));
		}
		p = strtok(NULL, "/");
		sprintf(_path, "%s/%s", _path, p);
	}
	
	return 0;
}

struct yd_upload_s {
	struct yd_data_t *d; 
	struct update_s * u;
	char key[128];
	void *value;
};

int yd_upload_delete_callback(void *user_data, char *error){
	struct yd_upload_s *s = user_data;
	struct yd_data_t *d = s->d; 
	struct update_s * u = s->u;	

	if (error){
		char from_path[BUFSIZ];
		sprintf(from_path, "app:/data/%s/%s", u->tablename, u->uuid);
		
		char to_path[BUFSIZ];
		sprintf(to_path, "app:/deleted/%s/%s", u->tablename, u->uuid);
		
		if(d->callback)
			d->callback(d->user_data, d->thread, STR("yd_upload: can't move %s to %s. Error: %s", from_path, to_path, error));
		return 0;
	}
	
	//remove from loacal updates table
	char SQL[BUFSIZ];	
	sprintf(SQL, "DELETE FROM kdata_updates WHERE uuid = '%s'", u->uuid);	
	int err = sqlite_connect_execute(SQL, d->database_path);
	if (err){
		if (d->callback)
			d->callback(d->user_data, d->thread, STR("yd_upload: %s. Error: %d", SQL, err));
		return 0;
	}	

	//callback
	if (d->callback)
		d->callback(d->user_data, d->thread, STR("yd_upload: in cloud removed %s from %s", u->uuid, u->tablename));

	return 0;
}

int yd_upload_data_callback(size_t size, void *user_data, char *error){
	struct yd_upload_s *s = user_data;
	struct yd_data_t *d = s->d; 
	struct update_s * u = s->u;	
	
	if (error){
		if (d->callback)
			d->callback(d->user_data, d->thread, STR("yd_upload: yd_upload_data_callback error: %s", error));		
		goto FREE;
	}
	
	if (d->callback)
		d->callback(d->user_data, d->thread, STR("yd_upload: uploaded size: %ld, table: %s, uuid: %s, timestamp: %ld, key: %s", size, u->tablename, u->uuid, u->timestamp, s->key));		

	//free
	FREE:
	free(s->value);
	free(u);
	free(s);

	return 0;
}

int yd_upload_callback(void *user_data, int argc, char **argv, char **titles){ 
	struct yd_upload_s *s = user_data;
	struct yd_data_t *d = s->d; 
	struct update_s * u = s->u;	

	int i;
	for (int i = 0; i < argc; i++) {
		if (argv[i] && titles[i]){
			//allocate args to upload data in thread
			struct update_s * _u = NEW(struct update_s);
			strcpy(_u->uuid, u->uuid);	
			strcpy(_u->tablename, u->tablename);	
			_u->timestamp = u->timestamp;
			_u->deleted = u->deleted;
			_u->localchange = u->localchange;			

			struct yd_upload_s *_s = NEW(struct yd_upload_s);
			_s->u = _u; _s->d = d;
			strncpy(_s->key, titles[i], 127); _s->key[127] = 0; 

			//allocate and copy data
			size_t len = strlen(argv[i]) + 1; //add +1 for \0 at the end of the string
			void * value = malloc(len);
			if (!value){
				if (d->callback)
					d->callback(d->user_data, d->thread, STR("yd_upload: value memory allocate error"));
				return 0;
			}
			memcpy(value, argv[i], len);
			_s->value = value; //save pointer as arg - to free value in callback

			//upload in thread and run callback
			char path[BUFSIZ];
			sprintf(path, "app:/data/%s/%s/%ld", u->tablename, u->uuid, u->timestamp);
			
			int err = c_yandex_disk_upload_data(
					d->token, 
					value, 
					len, 
					path, 
					true, 
					true, 
					&_s, 
					yd_upload_data_callback, 
					NULL, 
					NULL
					);

			if (err){
				if (d->callback)
					d->callback(d->user_data, d->thread, STR("yd_upload: c_yandex_disk_upload_data error: %d", err));
				return 1; //stop execution of sqlite_connect_execute_function
			}			
		}
	}

	return 0;
}

void yd_upload(
		struct yd_data_t *d, 
		struct update_s * u
		)
{
	struct yd_upload_s s = {.d=d, u=u};
	int err = 0;
	if (u->deleted){
		//delete from cloud data
		char from_path[BUFSIZ];
		sprintf(from_path, "app:/data/%s/%s", u->tablename, u->uuid);
		
		char to_path[BUFSIZ];
		sprintf(to_path, "app:/deleted/%s", u->tablename);
		//create directories
		create_directories(d->token, to_path);
		sprintf(to_path, "%s/%s", to_path, u->uuid);
		
		err = c_yandex_disk_mv(
				d->token, 
				from_path, 
				to_path, 
				true, 
				&s, 
				yd_upload_delete_callback
				);
		if (err){
			if (d->callback)
				d->callback(d->user_data, d->thread, STR("yd_upload: c_yandex_disk_mv error: %d", err));
			return;
		}		
	
	} else {
		//create path
		char path[BUFSIZ];
		sprintf(path, "app:/data/%s/%s/%ld", u->tablename, u->uuid, u->timestamp);
		create_directories(d->token, path);
		
		//upload to cloud data
		char SQL[BUFSIZ];
		sprintf(SQL, "SELECT * FROM %s WHERE uuid = '%s'", u->tablename, u->uuid);
		err = sqlite_connect_execute_function(SQL, d->database_path, &s, yd_upload_callback);
		if (err){
			if (d->callback)
				d->callback(d->user_data, d->thread, STR("yd_upload: %s. Error: %d", SQL, err));
			return;
		}				

		//callback
		if (d->callback)
			d->callback(d->user_data, d->thread, STR("yd_upload: uploaded data for: %s in: %s", u->uuid, u->tablename));		
	}

}

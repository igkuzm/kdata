/**
 * File              : yd_download.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 29.07.2022
 * Last Modified Date: 21.09.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

#include "kdata.h"
#include "cYandexDisk/cYandexDisk.h"
#include "SQLiteConnect/SQLiteConnect.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NEW(T) ({T *new = malloc(sizeof(T)); new;})
#define STR(...) ({char str[BUFSIZ]; snprintf(str,BUFSIZ-1,__VA_ARGS__); str[BUFSIZ-1]=0; str;})

struct yd_download_s {
	struct yd_data_t *d;
	struct update_s  *u;
	char key[128];
};

int yd_download_data_callback(size_t size, void *data, void *user_data, char *error){
	struct yd_download_s *s = user_data;
	struct yd_data_t *d = s->d;
	struct update_s *u = s->u;
	
	if (size){
		char * SQL  = malloc(BUFSIZ + size);	
		if (SQL == NULL){
			if(d->callback)
				d->callback(d->user_data, d->thread, "yd_download_data_callback error: SQL malloc");
			free(u);
			free(s);
			return 0;			
		}

		snprintf(SQL,
				BUFSIZ + size,
				"INSERT INTO %s (uuid) "
				"SELECT '%s' "
				"WHERE NOT EXISTS (SELECT 1 FROM %s WHERE uuid = '%s'); "
				"UPDATE %s SET '%s' = '%s' WHERE uuid = '%s'"
				,
				u->tablename, 
				u->uuid,
				u->tablename, u->uuid,
				u->tablename, s->key, (char *)data, u->uuid		
		);
		int err = sqlite_connect_execute(SQL, d->database_path);
		if (err){
			if(d->callback)
				d->callback(d->user_data, d->thread, STR("yd_download_data_callback error: %s", SQL));
			free(SQL);
			free(u);
			free(s);
			return 0;
		}
		free(SQL);
			
		//make callback		
		if(d->callback)
			d->callback(d->user_data, d->thread, 
					STR(
						"UPDATED: "
						"size: %ld, "
						"key: %s, "
						"uuid: %s, "
						"table: %s"
						, size, s->key, u->uuid, u->tablename)
					);
	}

	//free user_data
	free(u);
	free(s);

	return 0;
}

int yd_download_callback(c_yd_file_t *file, void *user_data, char *error){
	struct yd_download_s *s = user_data;
	struct yd_data_t *d = s->d;
	struct update_s *u = s->u;

	if (error){
		if (d->callback)
			d->callback(d->user_data, d->thread, STR("yd_download: %s", error));
		return 0;
	}

	if (file && strcmp("uuid", file->name)) { //dont copy uuid
		//allocate new arguments for download data in thread
		struct update_s *_u = NEW(struct update_s);
		strcpy(_u->uuid, u->uuid);	
		strcpy(_u->tablename, u->tablename);	
		_u->timestamp = u->timestamp;
		_u->deleted = u->deleted;
		_u->localchange = u->localchange;

		struct yd_download_s * _s = NEW(struct yd_download_s);
		_s->u = _u; _s->d = d;
		strncpy(_s->key, file->name, 127); _s->key[127] = 0; 

		//download data and run callback
		char path[BUFSIZ];
		sprintf(path, "app:/data/%s/%s/%ld/%s", u->tablename, u->uuid, u->timestamp, file->name);
		c_yandex_disk_download_data(
				d->token, 
				path, 
				true, 
				_s, 
				yd_download_data_callback, 
				NULL, 
				NULL
				);
	}

	return 0;
}

void yd_download(
			struct yd_data_t *d,
			struct update_s * u
		)
{
	int err = 0;
	char SQL[BUFSIZ];
	if (u->deleted){
		//delete from local data
		sprintf(SQL, "DELETE FROM %s WHERE uuid = '%s'", u->tablename, u->uuid);	
		err = sqlite_connect_execute(SQL, d->database_path);
		if (err){
			if (d->callback)
				d->callback(d->user_data, d->thread, STR("yd_download: %s. Error: %d", SQL, err));
			return;
		} 

		sprintf(SQL, "DELETE FROM kdata_updates WHERE uuid = '%s'", u->uuid);	
		err = sqlite_connect_execute(SQL, d->database_path);
		if (err){
			if (d->callback)
				d->callback(d->user_data, d->thread, STR("yd_download: %s. Error: %d", SQL, err));
			return;
		}

	} else {
		//download to local data
		char path[BUFSIZ];
		sprintf(path, "app:/data/%s/%s/%ld", u->tablename, u->uuid, u->timestamp);
		struct yd_download_s s = {.d = d, .u = u};
		err = c_yandex_disk_ls(d->token, path, &s, yd_download_callback);	
		if (err){
			if (d->callback)
				d->callback(d->user_data, d->thread, STR("yd_download: c_yandex_disk_ls %s. Error: %d", path, err));
			return;
		}			
		sprintf(SQL,
				"INSERT INTO kdata_updates (uuid) "
				"SELECT '%s' "
				"WHERE NOT EXISTS (SELECT 1 FROM kdata_updates WHERE uuid = '%s'); "
				"UPDATE kdata_updates SET table = '%s', deleted = 0, localchange = 0, timestamp = %ld WHERE uuid = '%s'"
				,
				u->uuid,
				u->uuid,
				u->tablename, u->timestamp, u->uuid		
		);		
		err = sqlite_connect_execute(SQL, d->database_path);
		if (err){
			if (d->callback)
				d->callback(d->user_data, d->thread, STR("yd_download: %s. Error: %d", SQL, err));
			return;
		}		
	}
}

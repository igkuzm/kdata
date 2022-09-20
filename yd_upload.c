/**
 * File              : yd_upload.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 03.05.2022
 * Last Modified Date: 20.09.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "yd.h"
#include "SQLiteConnect/SQLiteConnect.h"
#include "cYandexDisk/cYandexDisk.h"

#define STR(...)\
	({char ___str[BUFSIZ]; snprintf(___str, BUFSIZ-1, __VA_ARGS__); ___str[BUFSIZ-1] = 0; ___str;})

int
create_directories(
		const char * token,
		const char * path,
		const char * tablename
		)
{
	//buffer for path
	char buf[BUFSIZ];
	strncpy(buf, path, BUFSIZ - 1); buf[BUFSIZ - 1] = '\0';
	//make directory for path - strtok to use path components
	char _path[BUFSIZ] = "app:";
	printf("We have a path path: %s\n", buf);
	char *p = strtok(buf, "/");
	while (p) {
		char *error = NULL;
		sprintf(_path, "%s/%s", _path, p);
		printf("make directories for path: %s\n", _path);
		c_yandex_disk_mkdir(token, _path, &error);
		if (error) {
			perror(error);
		}
		p = strtok(NULL, "/");
	}
	
	//make directory for table
	char *error = NULL;
	char tablepath[BUFSIZ]; 
	sprintf(tablepath, "%s/%s", _path, tablename);
	printf("make directories for table: %s\n", tablepath);
	c_yandex_disk_mkdir(token, tablepath, &error);
	if (error) {
		perror(error);
	}	

	return 0;
}

struct upload_value_for_key_t {
	void * value;
	void *user_data;
	int (*callback)(size_t size, void *user_data, char *error);
};

int upload_value_for_key_callback(size_t size, void *user_data, char *error){
	//this is needed to free value and call callback
	struct upload_value_for_key_t * t = user_data;
	t->callback(size, t->user_data, error);

	free(t->value);
	free(t);

	return 0;
}

void
upload_value_for_key(
		const char * token,
		const char * path,
		const char * tablename,
		const char * identifier,
		time_t timestamp,
		void * value,
		size_t size,
		const char * key,
		void *user_data,           //pointer of data to transfer throw callback
		int (*callback)(		   //callback function when upload finished 
			size_t size,           //size of uploaded file
			void *user_data,       //pointer of data return from callback
			char *error			   //error
			)		
		)
{
	create_directories(token, path, tablename);
	
	//create directory for identifier
	char rowpath[BUFSIZ];
	sprintf(rowpath, "app:/%s/%s/%s", path, tablename, identifier);
	char *error = NULL;
	c_yandex_disk_mkdir(token, rowpath, &error);
	if (error) {
		perror(error);
	}	

	//create directory for timestamp
	sprintf(rowpath, "%s/%ld", rowpath, timestamp);
	error = NULL;
	c_yandex_disk_mkdir(token, rowpath, &error);
	if (error) {
		perror(error);
	}	

	//upload data for key
	struct upload_value_for_key_t * t = malloc(sizeof(struct upload_value_for_key_t));
	if (t==NULL){
		perror("upload_value_for_key_t malloc");
		if (callback)
			callback(0, user_data, "Can't allocate memory for upload_value_for_key_t");
		return;
	}
	t->value = malloc(size);
	if (t->value == NULL){
		perror("value malloc");
		if (callback)
			callback(0, user_data, "Can't allocate memory for value");
		return;		
	}
	memcpy(t->value, value, size);
	t->user_data = user_data;
	t->callback = callback;

	char keypath[BUFSIZ];
	sprintf(keypath, "%s/%s", rowpath, key);
	c_yandex_disk_upload_data(
			token, 
			value, 
			size, 
			keypath, 
			true,
			false,
			t, 
			upload_value_for_key_callback, 
			NULL, 
			NULL
			);
}


struct sqlite2yandexdisk_upload_d{
	int (*callback)(size_t size, void *user_data, char *error);			
	void *user_data;
	const char * token;
	const char * path;
	const char * database;
	const char * tablename;
	const char * uuid;		
	time_t timestamp;
};

int sqlite2yandexdisk_upload_callback(void *data, int argc, char **argv, char **titles) {
	struct sqlite2yandexdisk_upload_d *d = data;

	int i;
	for (i = 0; i < argc; ++i) {
		if (argv[i]){
			if (strcmp("uuid", argv[i])){ //don't use uuid column
				printf("sqlite2yandexdisk_upload: upload data for value: %s\n", titles[i]);
				upload_value_for_key(
					d->token,
					d->path,
					d->tablename,
					d->uuid,
					d->timestamp,
					argv[i],
					strlen(argv[i]) + 1, //save zero in the end of string
					titles[i],
					d->user_data,
					d->callback
				);
			}
		}
	}
	

	return 0;
}

void
sqlite2yandexdisk_upload(
		const char * token,
		const char * path,
		const char * database,
		const char * tablename,
		const char * uuid,		
		time_t timestamp,		   //last change of local data
		void *user_data,		   //pointer of data return from callback
		int (*callback)(		   //callback function when upload finished 
			size_t size,           //size of uploaded file
			void *user_data,       //pointer of data return from callback
			char *error			   //error
			)		
		)
{
	//get data from table and upload data 
	struct sqlite2yandexdisk_upload_d d = {
		.token = token,
		.path = path,
		.database = database,
		.tablename = tablename,
		.uuid = uuid,
		.timestamp = timestamp,
		.user_data = user_data,
		.callback = callback
	};	

	char SQL[BUFSIZ];
	sprintf(SQL, "SELECT * FROM %s WHERE uuid ='%s'", tablename, uuid);
	sqlite_connect_execute_function(SQL, database, &d, sqlite2yandexdisk_upload_callback);
}

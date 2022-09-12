/**
 * File              : kdata.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 11.09.2022
 * Last Modified Date: 12.09.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

#include "kdata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "yd.h"
#include "SQLiteConnect/SQLiteConnect.h"
#include "cYandexDisk/uuid4/uuid4.h"


const char * kdata_parse_kerr(kerr err){
	switch (err) {
		case KERR_NOERR: return "no error";
		case KERR_ENOMEM: return "not enough memory";
		case KERR_NOFILE: return "no such file or directory";
		case KERR_DTYPE: return "error of data type";
		case KERR_DONTUSEUUID: return "error key can't be 'uuid' - it is used";
		case KERR_DONTUSEKDATAUPDATES: return "error table name can't be 'kdata_updates' - it is used";
		case KERR_SQLITE_CREATE: return "SQLite: can't create/access database";
		case KERR_SQLITE_EXECUTE: return "SQLite: execute error";
		case KERR_NULLSTRUCTURE: return "data structure is NULL";
	}
	return "";
}

kdata_t * kdata_table_init(){
	//allocate memory
	kdata_t * s = malloc(sizeof(kdata_t));
	if (!s) 
		return NULL;
	s->next = NULL;
	return s;
}

kerr kdata_table_add(kdata_t * t, DTYPE type, const char * key){
	if (!t) //ckeck if strucuture null
		return KERR_NULLSTRUCTURE;

	if (!strcmp(key, "uuid")) //dont use name 'uuid' for key
		return KERR_DONTUSEUUID;

	//create new table
	kdata_t * n = kdata_table_init();
	if (!n) 
		return KERR_ENOMEM;

	n->next = t;
	n->type = type;
	strncpy(n->key, key, sizeof(n->key) - 1); 
	n->key[sizeof(n->key) - 1] = 0;

	t = n; //set pointer of data to new table

	return KERR_NOERR;
}

kerr kdata_table_free(kdata_t * t){
	if (!t) //ckeck if strucuture null
		return KERR_NULLSTRUCTURE;
	
	while (t) {
		kdata_t * ptr = t;
		t = ptr->next;
		free(ptr);
	}
	
	return KERR_NOERR;
}

kdata_s * kdata_structure_init(){
	//allocate
	kdata_s * s = malloc(sizeof(kdata_s));
	if (!s) 
		return NULL;
	s->next = NULL;
	return s;	
}

kerr kdata_structure_add(kdata_s * s, kdata_t * table, const char * tablename){
	if (!s) //ckeck if strucuture null
		return KERR_NULLSTRUCTURE;

	if (!strcmp(tablename, "kdata_updates")) //dont use name 'kdata_updates' for key
		return KERR_DONTUSEKDATAUPDATES;
	
	kdata_s * n = kdata_structure_init();
	if (!n) //ckeck if null
		return KERR_ENOMEM;

	n->next = s;
	n->table = table;
	strncpy(n->tablename, tablename, sizeof(n->tablename) - 1); 
	n->tablename[sizeof(n->tablename) - 1] = 0;

	s = n; //change pointer to new

	return KERR_NOERR;	
}

kerr kdata_structure_free(kdata_s * s){
	if (!s)
		return KERR_NULLSTRUCTURE;
	
	while (s) {
		kdata_s * ptr = s;
		s = ptr->next;
		free(ptr);
	}
	
	return KERR_NOERR;	
}


kerr kdata_init(const char * filepath, kdata_s * s, DSERVICE service, const char * token,
		void * user_data, int (*daemon_callback)(void * user_data, pthread_t thread, char * msg)
		){
	//ceate database
	int res = sqlite_connect_create_database(filepath);
	if (res) 
		return KERR_SQLITE_CREATE;

	//create table to store update information
	char SQL[] = "CREATE TABLE IF NOT EXISTS "
				 "kdata_updates "
				 "( "
				 "uuid TEXT, "
				 "tablename TEXT, "
				 "timestamp INT, "
				 "localchange INT,"
				 "deleted INT"
				 ")"
	;
	res = sqlite_connect_execute(SQL, filepath);
	if (res) 
		return KERR_SQLITE_EXECUTE;	
	
	//create table for each in strucuture
	while (s) {
		kdata_t * t = s->table;
		if (t){
			char SQL[BUFSIZ] = "CREATE TABLE IF NOT EXISTS ";
			strcat(SQL, s->tablename);
			strcat(SQL, " ( ");
			//for each data type
			while(t) {
				char * type;
				switch (t->type) {
					case DTYPE_INT : type="INT" ; break;
					case DTYPE_TEXT: type="TEXT"; break;
					case DTYPE_DATA: type="BLOB"; break;
				}

				char str[256];
				sprintf(str, "%s %s, ", t->key, type);
				strcat(SQL, str);
				
				t = t->next;
			}

			//add uuid key to table
			strcat(SQL, "uuid TEXT )");

			int res = sqlite_connect_execute(SQL, filepath);
			if (res) 
				return KERR_SQLITE_EXECUTE;
		}
		
		s = s->next;
	}

	//start daemon
	kdata_daemon_init(filepath, service, token, user_data, daemon_callback);	
	
	return KERR_NOERR;	
}

char * kdata_new_uuid(){
	//create uuid
	char * uuid = malloc(37);
	if (!uuid)
		return NULL;
	UUID4_STATE_T state; UUID4_T identifier;
	uuid4_seed(&state);
	uuid4_gen(&state, &identifier);
	if (!uuid4_to_s(identifier, uuid, 37)){
		return NULL;
	}

	return uuid;
}

void
update_timestamp_for_uuid(const char * filepath, const char * tablename, const char * uuid, int deleted){
	time_t timestamp = time(NULL);
	char SQL[BUFSIZ];
	sprintf(SQL,
			"INSERT INTO kdata_updates (uuid) "
			"SELECT '%s' "
			"WHERE NOT EXISTS (SELECT 1 FROM kdata_updates WHERE uuid = '%s'); "
			"UPDATE kdata_updates SET timestamp = %ld, tablename = '%s', localchange = 1, deleted = %d WHERE uuid = '%s'"
			,
			uuid,
			uuid,
			timestamp, tablename, deleted, uuid		
	);
	sqlite_connect_execute(SQL, filepath);	
}

void kdata_add(
		const char * filepath, 
		const char * tablename, 
		void * user_data,
		int (*callback)(void * user_data, char * uuid, kerr err)
		){
	
	char * uuid = kdata_new_uuid();
	if (!uuid){ //can not generate uuid
		if (callback)
			callback(user_data, NULL, KERR_ENOMEM);
		return;
	}

	//insert into table
	char SQL[BUFSIZ];
	sprintf(SQL, 
			"INSERT INTO %s (uuid) VALUES ('%s')",
			tablename, uuid);	

	int res = sqlite_connect_execute(SQL, filepath);
	if (res){
		if (callback)
			callback(user_data, NULL, KERR_SQLITE_EXECUTE);
		return;		
	}

	//callback uuid for inserted item
	if (callback)
		callback(user_data, uuid, KERR_NOERR);
	
	//free uuid
	free(uuid);
}

kerr kdata_remove(
		const char * filepath, 
		const char * tablename, 
		const char * uuid
		){
	
	//remove row
	char SQL[BUFSIZ];
	sprintf(SQL, 
			"DELETE FROM %s WHERE uuid = '%s'",
			tablename, uuid);
	
	int res = sqlite_connect_execute(SQL, filepath);
	if (res)
		return KERR_SQLITE_EXECUTE;

	//update kdata_update table
	update_timestamp_for_uuid(filepath, tablename, uuid, 1);
	
	return KERR_NOERR;	
}	

kerr kdata_set_int_for_key(
		const char * filepath, 
		const char * tablename, 
		const char * uuid, 
		int value, 
		const char * key
		){
	
	char SQL[BUFSIZ];
	sprintf(SQL, 
			"UPDATE %s SET %s = %d WHERE uuid = '%s'",
			tablename, key, value, uuid);
	
	int res = sqlite_connect_execute(SQL, filepath);
	if (res)
		return KERR_SQLITE_EXECUTE;

	//update kdata_update table
	update_timestamp_for_uuid(filepath, tablename, uuid, 0);
	
	return KERR_NOERR;	
}

kerr kdata_set_text_for_key(
		const char * filepath, 
		const char * tablename, 
		const char * uuid, 
		const char * text, 
		const char * key
		){
	
	char SQL[TEXTMAXSIZE];
	sprintf(SQL, 
			"UPDATE %s SET %s = '%s' WHERE uuid = '%s'",
			tablename, key, text, uuid);
	
	int res = sqlite_connect_execute(SQL, filepath);
	if (res)
		return KERR_SQLITE_EXECUTE;

	//update kdata_update table
	update_timestamp_for_uuid(filepath, tablename, uuid, 0);
	
	return KERR_NOERR;	
}


void
kdata_daemon_init(
			const char * filepath,
			DSERVICE service,
			const char * token,
			void * user_data,
			int (*callback)(void * user_data, pthread_t thread, char * msg)
		)
{
	if (service == DSERVICE_LOCAL){
		if (callback)
			callback(user_data, NULL, "kdata daemon: local only");
		return;
	}

	if (service == DSERVICE_YANDEX){
		yd_daemon_init(filepath, token, user_data, callback);
		return;
	}
}

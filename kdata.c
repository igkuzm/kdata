/**
 * File              : kdata.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 11.09.2022
 * Last Modified Date: 19.09.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

#include "kdata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "yd.h"
#include "SQLiteConnect/SQLiteConnect.h"
#include "cYandexDisk/uuid4/uuid4.h"
#include "base64.h"


const char * kdata_parse_kerr(kerr err){
	switch (err) {
		case KERR_NOERR: return "no error";
		case KERR_ENOMEM: return "not enough memory";
		case KERR_NOFILE: return "no such file or directory";
		case KERR_NODATA: return "no data to send";
		case KERR_BASE64: return "base64 encode/decode error";
		case KERR_DTYPE: return "error of data type";
		case KERR_DONTUSEUUID: return "error key can't be 'uuid' - it is used";
		case KERR_DONTUSEKDATAUPDATES: return "error table name can't be 'kdata_updates' - it is used";
		case KERR_SQLITE_CREATE: return "SQLite: can't create/access database";
		case KERR_SQLITE_EXECUTE: return "SQLite: execute error";
		case KERR_NULLSTRUCTURE: return "data structure is NULL";
	}
	return "";
}

void kdata_table_init(kdata_table * t, const char * tablename, ...){
	//set tablename
	strncpy(t->tablename, tablename, sizeof(t->tablename) - 1);
	t->tablename[sizeof(t->tablename) - 1] = 0;

	//allocate columns array
	t->columns = malloc(sizeof(kdata_column));
	if (!t->columns)
		return;
	
	int count = 0;
	
	//init va_args
	va_list args;
	va_start(args, tablename);

	DTYPE type = va_arg(args, DTYPE);
	if (type == DTYPE_NONE)
		return;

	char * key = va_arg(args, char *);
	if (!key)
		return;

	//iterate va_args
	while (type != DTYPE_NONE && key != NULL){
		
		//new column
		kdata_column c = {.type = type};
		strcpy(c.key, key);

		//add column to array
		t->columns[count] = c;
		count++;

		//realloc columns array
		t->columns = realloc(t->columns, (sizeof(kdata_column) + sizeof(kdata_column) * count));
		if (!t->columns)
			return;
		
		type = va_arg(args, DTYPE);
		key = va_arg(args, char *);
		if (!key) 
			break;
		
	}
	t->columns_count = count;
}


void kdata_table_free(kdata_table * t) {
	free(t->columns);
}

void kdata_d_init(kdata_d * value){
	value->type = DTYPE_NONE;
	value->key[0] = 0;
	value->int_value = 0;
	value->text_value[0] = 0;
	value->data_value = NULL;
	value->data_len = 0;
} 

kdata_s * kdata_structure_init(){
	//allocate
	kdata_s * s = malloc(sizeof(kdata_s));
	if (!s) 
		return NULL;
	s->next = NULL;
	return s;	
}

kerr kdata_structure_add(
		kdata_s ** s, 
		kdata_table *t		
		)
{
	if (!s) //ckeck if strucuture null
		return KERR_NULLSTRUCTURE;

	if (!strcmp(t->tablename, "kdata_updates")) //dont use name 'kdata_updates' for key
		return KERR_DONTUSEKDATAUPDATES;

	kdata_s * ptr = *s;

	kdata_s * n = kdata_structure_init();
	if (!n) //ckeck if null
		return KERR_ENOMEM;

	n->next = ptr;	
	n->table = *t;
	
	*s = n; //change pointer to new

	return KERR_NOERR;	
}

kerr kdata_structure_free(kdata_s * s){
	if (!s)
		return KERR_NULLSTRUCTURE;
	
	while (s) {
		kdata_s * ptr = s;
		s = ptr->next;
		kdata_table_free(&ptr->table);
		free(ptr);
	}
	
	return KERR_NOERR;	
}


kerr kdata_init(const char * filepath, kdata_s * s, DSERVICE service, const char * token,
		void * user_data, int (*daemon_callback)(void * user_data, pthread_t thread, char * msg)
		){
	//ceate database
	sqlite_connect_create_database(filepath);

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
	int res = sqlite_connect_execute(SQL, filepath);
	if (res) 
		return KERR_SQLITE_EXECUTE;	
	
	//create table for each in strucuture
	while (s) {
		kdata_table t = s->table;
		if (t.columns_count > 0){
			char SQL[BUFSIZ] = "CREATE TABLE IF NOT EXISTS ";
			strcat(SQL, t.tablename);
			strcat(SQL, " ( ");
		
			int i;
			for (i = 0; i < t.columns_count; i++) {
				kdata_column c = t.columns[i];

				if (strcmp("uuid", c.key)) { //check if key is not 'uuid'
					char * type;
					switch (c.type) {
						case DTYPE_INT : type="INT" ; break;
						case DTYPE_TEXT: type="TEXT"; break;
						case DTYPE_DATA: type="BLOB"; break;
						default: break;
					}

					char str[256];
					sprintf(str, "%s %s, ", c.key, type);
					strcat(SQL, str);
				}
			}

			//add uuid key to table
			strcat(SQL, "uuid TEXT )");

			int res = sqlite_connect_execute(SQL, filepath);
			if (res) 
				return KERR_SQLITE_EXECUTE;
		}
		
		//iterate database structure
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

kerr kdata_set_data_for_key(
		const char * filepath, 
		const char * tablename, 
		const char * uuid, 
		void * data, 
		size_t len, 
		const char * key
		){

	//check data len
	if (!len)
		return KERR_NODATA;
		
	//create base64 encoded data
	size_t size;
	char *base64 = base64_encode(data, len, &size);
	if (!size)
		return KERR_BASE64;
	
	//allocate memory
	char * SQL = malloc(BUFSIZ + size);
	if (!SQL)
		return KERR_ENOMEM;	

	//generete SQL string
	sprintf(SQL, 
			"UPDATE %s SET %s = '%s' WHERE uuid = '%s'",
			tablename, key, base64, uuid);
	
	int res = sqlite_connect_execute(SQL, filepath);
	
	//free memory
	free(base64);
	free(SQL);
	
	//return error
	if (res)
		return KERR_SQLITE_EXECUTE;
	
	//update kdata_update table
	update_timestamp_for_uuid(filepath, tablename, uuid, 0);
	
	return KERR_NOERR;	
}

struct kdata_for_each_t {
	void * user_data;
	const kdata_table * table;
	int (*callback) (void * user_data, int argc, kdata_d * argv, kerr err);
};

int kdata_for_each_callback(
		void *user_data, 
		int argc, 
		char *argv[], 
		char *titles[]
		)
{
	struct kdata_for_each_t *t = user_data;

	//allocate row
	kdata_d * a = malloc(sizeof(kdata_d) * argc);
	if (!a){
		if (t->callback)
			t->callback(t->user_data, 0, NULL, KERR_ENOMEM);
		return 0;
	}

	//iterate columns
	for (int i = 0; i < argc; ++i) {
		
		kdata_d value;
		kdata_d_init(&value);
		if (titles[i])
			strcpy(value.key, titles[i]);

		kdata_column col = t->table->columns[i];

		if (argv[i] != NULL) {
			switch (col.type) {
				case DTYPE_INT:  
					value.int_value = atoi(argv[i]); 
					break;

				case DTYPE_TEXT: 
					strncpy(value.text_value, argv[i], TEXTMAXSIZE - 1);
					value.text_value[TEXTMAXSIZE - 1] = '\0';						
					break;

				case DTYPE_DATA:  				
					{
						size_t len = strlen(argv[i]);
						size_t size;
						unsigned char * data = base64_decode(argv[i], len, &size);
						value.data_len = size;
						value.data_value = data;
					}
					break;

				default:
					break;
			}
		}

		//add column to row
		a[i] = value;
	}

	//callback row
	if (t->callback)
		t->callback(t->user_data, argc, a, KERR_NOERR);

	//free memory
	free(a);

	return 0;
}

void kdata_for_each(
		const char * filepath, 
		const kdata_table * table,
		const char * predicate, 
		void * user_data,
		int (*callback)(
			void * user_data, 
			int argc,
			kdata_d * values, 
			kerr err)
		)
{
	//check table
	if (!table){
		if (callback)
			callback(user_data, 0, NULL, KERR_NODATA);
		return;		
	}

	//make user_data for callback
	struct kdata_for_each_t t = {
		.user_data = user_data,
		.callback = callback,
		.table = table
	};

	//generate SQL string
	char SQL[BUFSIZ];
	sprintf(SQL, "SELECT * FROM %s ", table->tablename);	
	if (predicate)
		strcat(SQL, predicate);
	
	//run sql query
	int res = sqlite_connect_execute_function(SQL, filepath, &t, kdata_for_each_callback);
	if (res){
		if (callback)
			callback(user_data, 0, NULL, KERR_SQLITE_EXECUTE);
	}	
};

void kdata_daemon_init(
			const char * filepath,
			DSERVICE service,
			const char * token,
			kdata_s * s, 
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
		yd_daemon_init(filepath, token, s, user_data, callback);
		return;
	}
}

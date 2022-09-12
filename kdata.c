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

kdata_s * kdata_structure_init(){
	//allocate
	kdata_s * s = malloc(sizeof(kdata_s));
	if (!s) 
		return NULL;
	s->next = NULL;
	return s;	
}

kerr kdata_structure_add(kdata_s * s, kdata_s table){
	if (!s) //ckeck if strucuture null
		return KERR_NULLSTRUCTURE;

	if (!strcmp(table.tablename, "kdata_updates")) //dont use name 'kdata_updates' for key
		return KERR_DONTUSEKDATAUPDATES;
	
	kdata_s * n = kdata_structure_init();
	if (!n) //ckeck if null
		return KERR_ENOMEM;

	n->next = s;
	n->columns_count = table.columns_count;
	//copy columns
	int i;
	for (int i = 0; i < n->columns_count; i++) {
		n->columns[i] = table.columns[i];	
	}
	strncpy(n->tablename, table.tablename, sizeof(n->tablename) - 1); 
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
		if (s->columns_count > 0){
			char SQL[BUFSIZ] = "CREATE TABLE IF NOT EXISTS ";
			strcat(SQL, s->tablename);
			strcat(SQL, " ( ");
		
			int i;
			for (int i = 0; i < s->columns_count; i++) {
				kdata_column t = s->columns[i];

				if (strcmp("uuid", t.key)) { //check if key is not 'uuid'
					char * type;
					switch (t.type) {
						case DTYPE_INT : type="INT" ; break;
						case DTYPE_TEXT: type="TEXT"; break;
						case DTYPE_DATA: type="BLOB"; break;
					}

					char str[256];
					sprintf(str, "%s %s, ", t.key, type);
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
	void *user_data;
	kdata_s * table;
	int (*callback) (void * user_data, int argc, kdata_d * argv, kerr err);
};

int 
kdata_for_each_callback(
		void *user_data, 
		int argc, 
		char *argv[], 
		char *titles[]
		)
{
	struct kdata_for_each_t *t = user_data;

	//allocate array of values
	kdata_d * a = malloc(sizeof(kdata_d) * argc);
	if (!a){
		if (t->callback)
			t->callback(t->user_data, 0, NULL, KERR_ENOMEM);
		return 0;
	}

	for (int i = 0; i < argc; ++i) {
		char buff[128];
		if (!argv[i]) buff[0] = '\0'; //no seg falt on null
		else {
			strncpy(buff, argv[i], 127);
			buff[127] = '\0';
		}

		switch (i) {
			case 0:  item.id = atoi(buff)                ; break;
			case 1:  strcpy(item.uuid, buff)             ; break;
			case 2:  item.date = atoi(buff)              ; break;
			case 3:  strcpy(item.name, buff)             ; break;
			case 4:  strcpy(item.latin_name, buff)       ; break;
			case 5:  strcpy(item.region, buff)           ; break;
			case 6:  strcpy(item.kategory, buff)         ; break;
			case 7:  strcpy(item.method, buff)           ; break;
			case 8:  strcpy(item.company, buff)          ; break;

			case 9:  strcpy(item.color, buff)            ; break;
			case 10: strcpy(item.clearness, buff)        ; break;
			case 11: strcpy(item.intensity, buff)        ; break;
			case 12: strcpy(item.transparence, buff)     ; break;
			case 13: strcpy(item.fluidity, buff)         ; break;
			case 14: strcpy(item.ductility, buff)        ; break;
			case 15: strcpy(item.volatility, buff)       ; break;
			case 16: strcpy(item.integrality, buff)      ; break;

			case 17: item.olfactory = atoi(buff)         ; break;

			case 18: strcpy(item.olfactory_fist, buff)   ; break;
			case 19: strcpy(item.olfactory_second, buff) ; break;
			case 20: strcpy(item.olfactory_third, buff)  ; break;
			case 21: strcpy(item.olfactory_last, buff)   ; break;

			case 22: strcpy(item.sin_color, buff)        ; break;
			case 23: strcpy(item.sin_sound, buff)        ; break;
			case 24: strcpy(item.sin_form, buff)         ; break;
			case 25: strcpy(item.sin_simbol, buff)       ; break;

			case 26: item.rating_phis = atoi(buff)       ; break;
			case 27: item.rating_aroma = atoi(buff)      ; break;
			case 28: item.rating_energy = atoi(buff)     ; break;

			case 29: strcpy(item.rating, buff)           ; break;

			//parse image
			case 30: 
				{
					item.image0_len = 0;
					item.image0 = NULL;
					if (argv[i] != NULL && argv[i+1] != NULL) {
						size_t len = atoi(argv[i+1]);
						size_t size;
						unsigned char * data = base64_decode(argv[i], len, &size);
						item.image0_len = size;
						item.image0 = data;
					}
				} 
				break;

			case 32: 
				{
					item.image1_len = 0;
					item.image1 = NULL;
					if (argv[i] != NULL && argv[i+1] != NULL) {
						size_t len = atoi(argv[i+1]);
						size_t size;
						unsigned char * data = base64_decode(argv[i], len, &size);
						item.image1_len = size;
						item.image1 = data;
					}
				} 
				break;				

			default:                                       break;
		}
	}

	if (t->callback)
		t->callback(&item, t->user_data, NULL);

	return 0;
}

void
ya_rating_for_each(
		const char * database,
		const char * predicate,
		void * user_data, 
		int (*callback) (
			struct ya_rating_t *item, 
			void *user_data, 
			char *error
			)
		)
{
	struct ya_rating_for_each_t t;
	t.user_data = user_data;
	t.callback = callback;

	char SQL[BUFSIZ];
	if (predicate) {
		sprintf(SQL, "SELECT * FROM rating WHERE %s", predicate);	
	} else {
	   	sprintf(SQL, "SELECT * FROM rating");
	}
	sqlite_connect_execute_function(SQL, database, &t, ya_rating_for_each_callback);
};










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



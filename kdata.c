/**
 * File              : kdata.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 11.09.2022
 * Last Modified Date: 11.09.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

#include "kdata.h"
#include <stdlib.h>
#include <string.h>
#include "sqlite2yandexdisk/sqlite2yandexdisk.h"
#include "sqlite2yandexdisk/SQLiteConnect/SQLiteConnect.h"

const char * kdata_parse_kerr(kerr err){
	switch (err) {
		case KERR_NOERR: return "no error";
		case KERR_ENOMEM: return "not enough memory";
		case KERR_NOFILE: return "no such file or directory";
		case KERR_DTYPE: return "error of data type";
		case KERR_SQLITE_CREATE: return "SQLite: can't create/access database";
		case KERR_NULLSTRUCTURE: return "data structure is NULL";
	}
	return "";
}

kdata_t * kdata_table_init(){
	kdata_t * s = malloc(sizeof(kdata_t));
	if (!s) 
		return NULL;
	s->next = NULL;
	return s;
}

kerr kdata_table_add(kdata_t * t, DTYPE type, const char * key){
	if (!t)
		return KERR_NULLSTRUCTURE;

	kdata_t * n = kdata_table_init();
	if (!n) 
		return KERR_ENOMEM;

	n->next = t;
	n->type = type;
	strncpy(n->key, key, sizeof(n->key) - 1); 
	n->key[sizeof(n->key) - 1] = 0;

	t = n;

	return KERR_NOERR;
}

kerr kdata_table_free(kdata_t * t){
	if (!t)
		return KERR_NULLSTRUCTURE;
	
	while (t) {
		kdata_t * ptr = t;
		t = ptr->next;
		free(ptr);
	}
	
	return KERR_NOERR;
}

kdata_s * kdata_structure_init(){
	kdata_s * s = malloc(sizeof(kdata_s));
	if (!s) 
		return NULL;
	s->next = NULL;
	return s;	
}

kerr kdata_structure_add(kdata_s * s, kdata_t * table, const char * tablename){
	if (!s)
		return KERR_NULLSTRUCTURE;

	kdata_s * n = kdata_structure_init();
	if (!n) 
		return KERR_ENOMEM;

	n->next = s;
	n->table = table;
	strncpy(n->tablename, tablename, sizeof(n->tablename) - 1); 
	n->tablename[sizeof(n->tablename) - 1] = 0;

	s = n;

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


kerr kdata_init(const char * filepath, kdata_t * structure, DSERVICE service, const char * token){
	int res = sqlite_connect_create_database(filepath);
	if (res) 
		return KERR_SQLITE_CREATE;
	
	char SQL[BUFSIZ] = "CREATE TABLE IF NOT EXISTS ";
}

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

const char * kdata_parse_kerr(kerr err){
	switch (err) {
		case KERR_NOERR: return "no error";
		case KERR_ENOMEM: return "not enough memory";
		case KERR_NOFILE: return "no such file or directory";
		case KERR_DTYPE: return "error of data type";
		case KERR_NULLSTRUCTURE: return "data structure is NULL";
	}
	return "";
}

kdata_t * kdata_structure_init(){
	kdata_t * s = malloc(sizeof(kdata_t));
	if (!s) 
		return NULL;
	s->next = NULL;
	return s;
}

kerr kdata_structure_add(kdata_t * s, DTYPE type, const char * key){
	if (!s)
		return KERR_NULLSTRUCTURE;

	kdata_t * n = kdata_structure_init();
	if (!n) 
		return KERR_ENOMEM;

	n->next = s;
	n->type = type;
	strncpy(n->key, key, sizeof(n->key) - 1); 
	n->key[sizeof(n->key) - 1] = 0;

	s = n;

	return KERR_NOERR;
}

kerr kdata_structure_free(kdata_t * s){
	if (!s)
		return KERR_NULLSTRUCTURE;
	
	while (s) {
		kdata_t * ptr = s;
		s = ptr->next;
		free(ptr);
	}
	
	return KERR_NOERR;
}

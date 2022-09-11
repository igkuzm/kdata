/**
 * File              : kdata.h
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 11.09.2022
 * Last Modified Date: 11.09.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

#ifndef k_data_h__
#define k_data_h__

#ifdef __cplusplus
extern "C"{
#endif

	#include <stdlib.h>

	/*! \enum DSERVICE
	*
	*  cloud data service (yandex_disk, google)
	*/
	typedef enum DSERVICE {
		DSERVICE_LOCAL,	
		DSERVICE_YANDEX   
	} DSERVICE;
	/*! \enum DTYPE
	*
	*  type of data (integer, text, data)
	*/
	typedef enum DTYPE {
		DTYPE_INT,
		DTYPE_TEXT,
		DTYPE_DATA
	} DTYPE;

	/*! \enum KERR
	*
	*  errors
	*/
	typedef enum KERR { 
		KERR_NOERR,
		KERR_ENOMEM,
		KERR_NOFILE,
		KERR_DTYPE,
		KERR_NULLSTRUCTURE
   	} kerr;
	
	//parse kerr to error
	const char * kdata_parse_kerr(kerr err, const char * error);
	
	//list of data structure
	typedef struct kdata_t {
		DTYPE type;
		char key[128];
		struct kdata_t * next;
	} kdata_t;

	//create new data structure
	kdata_t * kdata_structure_init(kdata_t * strucuture);

	//add type for key to data strucuture
	kerr kdata_structure_add(kdata_t * structure, DTYPE type, const char * key);


	//init database (SQLite) at filepath (create if needed) with structure and start cloud service
	kerr kdata_init(const char * filepath, kdata_t * structure, DSERVICE service, const char * token);

	//set int value for table column (key). set uuid to null - to create new row
	kerr kdata_set_int_for_key(const char * filepath, const char * tablename, const char * uuid, int value, const char * key);

	//set text value with len for table column (key). set uuid to null - to create new row
	kerr kdata_set_text_for_key(const char * filepath, const char * tablename, const char * uuid, const char * text, unsigned long len, const char * key);

	//set data value with len for table column (key). set uuid to null - to create new row
	kerr kdata_set_data_for_key(const char * filepath, const char * tablename, const char * uuid, void * data, unsigned long len, const char * key);

	//get int for key
	void kdata_get_int_for_key(const char * filepath, const char * tablename, const char * uuid, void * user_data,
			int (*callback)(void * user_data, int value, kerr err));


#ifdef __cplusplus
}
#endif

#endif //k_data_h__




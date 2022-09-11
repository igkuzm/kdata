/**
 * File              : kdata.h
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 11.09.2022
 * Last Modified Date: 12.09.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

#ifndef k_data_h__
#define k_data_h__

#ifdef __cplusplus
extern "C"{
#endif

	#include <stdlib.h>

#ifndef __ANDROID__
	#include <pthread.h>
#endif

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
		KERR_DONTUSEUUID,
		KERR_DONTUSEKDATAUPDATES,
		KERR_SQLITE_CREATE,
		KERR_SQLITE_EXECUTE,
		KERR_NULLSTRUCTURE
   	} kerr;
	
	//parse kerr to error
	const char * kdata_parse_kerr(kerr err);
	
	//list of table structure
	typedef struct kdata_t {
		DTYPE type;
		char key[128];
		struct kdata_t * next;
	} kdata_t;

	//create new table structure
	kdata_t * kdata_table_init();

	//add type for key to table strucuture
	kerr kdata_table_add(
			kdata_t * table, 
			DTYPE type, 
			const char * key
	);
	
	//free table structure
	kerr kdata_table_free(kdata_t * table);

	//list of data structure
	typedef struct kdata_s {
		struct kdata_t * table;
		char tablename[128];
		struct kdata_s * next;
	} kdata_s;

	//create new data structure
	kdata_s * kdata_structure_init();

	//add table to data strucuture
	kerr kdata_structure_add(
			kdata_s * strucuture, 
			kdata_t * table, 
			const char * tablename
	);
	
	//free data structure
	kerr kdata_structure_free(kdata_s * strucuture);	


	//init database (SQLite) at filepath (create if needed) with structure and start cloud service
	kerr kdata_init(
			const char * filepath, 
			kdata_s * structure, 
			DSERVICE service, 
			const char * token,
			void * user_data,
			int (*daemon_callback)(
				void * user_data, 
				pthread_t thread, 
				char * msg)			
	);

	//add row
	void kdata_add(
			const char * filepath, 
			const char * tablename, 
			void * user_data,
			int (*callback)(void * user_data, char * uuid, kerr err)
	);

	//remove row with uuid
	kerr kdata_remove(
			const char * filepath, 
			const char * tablename, 
			const char * uuid 
	);	
	
	//set int value for table column (key)
	kerr kdata_set_int_for_key(
			const char * filepath, 
			const char * tablename, 
			const char * uuid, 
			int value, 
			const char * key
	);

	//set text value for table column (key). set uuid to null - to create new row
	kerr kdata_set_text_for_key(
			const char * filepath, 
			const char * tablename, 
			const char * uuid, 
			const char * text, 
			const char * key
	);

	//set data value with len for table column (key). set uuid to null - to create new row
	kerr kdata_set_data_for_key(
			const char * filepath, 
			const char * tablename, 
			const char * uuid, 
			void * data, 
			unsigned long len, 
			const char * key
	);

	//get int for key
	void kdata_get_int_for_key(
			const char * filepath, 
			const char * tablename, 
			const char * uuid, 
			void * user_data,
			int (*callback)(
				void * user_data, 
				int value, 
				kerr err)
	);


	void kdata_daemon_init(
			const char * filepath,
			DSERVICE service,
			const char * token,
			void * user_data,
			int (*callback)(
				void * user_data, 
				pthread_t thread, 
				char * msg)
	);

	void yd_daemon_init(
				const char * filepath,
				const char * token,
				void * user_data,
				int (*callback)(
					void * user_data, 
					pthread_t thread, 
					char * msg)
	);	
	

#ifdef __cplusplus
}
#endif

#endif //k_data_h__




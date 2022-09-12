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

#define TEXTMAXSIZE 2048

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
		KERR_NODATA,
		KERR_BASE64,
		KERR_DTYPE,
		KERR_DONTUSEUUID,
		KERR_DONTUSEKDATAUPDATES,
		KERR_SQLITE_CREATE,
		KERR_SQLITE_EXECUTE,
		KERR_NULLSTRUCTURE
   	} kerr;
	
	//parse kerr to error
	const char * kdata_parse_kerr(kerr err);
	
	//column
	typedef struct kdata_column {
		DTYPE type;
		char key[128];
	} kdata_column;

	//database table structure
	typedef struct kdata_s {
		int columns_count;
		kdata_column * columns;
		char tablename[128];
		struct kdata_s * next;
	} kdata_s;

	//create new data structure
	kdata_s * kdata_structure_init();

	//add table to data strucuture
	kerr kdata_structure_add(
		kdata_s * strucuture, 
		kdata_s table 
	);
	
	//free data structure
	kerr kdata_structure_free(kdata_s * strucuture);	

	//list of data values
	typedef struct kdata_d {
		DTYPE type;
		char key[128];
		int int_value;
		char * text_value;
		void * data_value;
		size_t data_len;
	} kdata_d;	

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
			size_t len, 
			const char * key
	);

	//get int for key
	void kdata_for_each(
			const char * filepath, 
			kdata_s table,
			const char * predicate, 
			void * user_data,
			int (*callback)(
				void * user_data, 
				int argc,
				kdata_d * values, 
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




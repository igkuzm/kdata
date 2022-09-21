/**
 * File              : kdata.h
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 11.09.2022
 * Last Modified Date: 21.09.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

#ifndef k_data_h__
#define k_data_h__

#ifdef __cplusplus
extern "C"{
#endif

	#include <stdio.h>
	#include <stdlib.h>
	#include <pthread.h>

#define SEC 300
#define PATH "data"
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
		DTYPE_NONE,
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
	typedef struct kdata_table {
		char tablename[128];
		int columns_count;
		kdata_column * columns;
	} kdata_table;

	//allocate table
	kdata_table * kdata_table_new();
	
	//new table with columns; va_args: type, key, ... NULL
	void kdata_table_init(kdata_table * t, const char * tablename, ...); 

	//free table
	void kdata_table_free(kdata_table * table);
	
	//database structure
	typedef struct kdata_s {
		struct kdata_table table;
		struct kdata_s * next;
	} kdata_s;	

	//create new database structure
	kdata_s * kdata_structure_init();

	//add table to data strucuture
	kerr kdata_structure_add(
		kdata_s ** strucuture, 
		kdata_table * table
	);
	
	//free data structure
	kerr kdata_structure_free(kdata_s * strucuture);	

	//list of data values
	typedef struct kdata_d {
		DTYPE type;
		char key[128];
		int int_value;
		char text_value[TEXTMAXSIZE];
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

	//iterate all rows for column
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
	);


	//init cloud service
	void kdata_daemon_init(
			const char * filepath,
			DSERVICE service,
			const char * token,
			kdata_s * structure, 
			void * user_data,
			int (*callback)(
				void * user_data, 
				pthread_t thread, 
				char * msg)
	);

	//init yandex disk cloud service
	void yd_daemon_init(
				const char * filepath,
				const char * token,
				kdata_s * structure, 
				void * user_data,
				int (*callback)(
					void * user_data, 
					pthread_t thread, 
					char * msg)
	);	

	struct yd_data_t{
		char database_path[BUFSIZ];
		char token[128];
		kdata_s * s; 
		void * user_data;
		pthread_t thread;
		int (*callback)(void * user_data, pthread_t thread, char * msg);
		char uuid[37];
		char tablename[256];
		time_t timestamp;
		int deleted;	
	};

	void 
	kdata_get_yd_update(
			const char * database,
			const char * token,
			kdata_s * structure,
			struct yd_data_t *yddata
			);	
	

#ifdef __cplusplus
}
#endif

#endif //k_data_h__




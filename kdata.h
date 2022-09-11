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
	enum DSERVICE { 
		YANDEX   
	};

	/*! \enum DTYPE
	*
	*  type of data (integer, text, data)
	*/
	enum DTYPE {
		DTYPE_INT,
		DTYPE_TEXT,
		DTYPE_DATA
	};

	//init database (SQLite) at filepath (create if needed) and start cloud service
	int kdata_init(const char * filepath, DSERVICE service, const char * token);



#ifdef __cplusplus
}
#endif

#endif //k_data_h__




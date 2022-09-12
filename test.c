/**
 * File              : test.c
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 12.09.2022
 * Last Modified Date: 12.09.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

#include "kdata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define DATABASE "my_data.db"

//table
void my_table_init(kdata_table * table) {
	kdata_table_init(table, "my_table", 
		DTYPE_INT,  "ID",
		DTYPE_TEXT, "NAME",
		DTYPE_INT,  "DATE",
		NULL
	);
}

int add_callback(void *user_data, char *uuid, kerr err){
	char **_uuid = user_data;
	if (err)
		printf("%s\n", kdata_parse_kerr(err));
	//else
		//strcpy(*_uuid, uuid);
	return 0;
}

int main(int argc, char *argv[])
{
	printf("Starting KDATA test...\n");
	
	//init database structure
	kdata_table my_table;
	my_table_init(&my_table);
	kdata_s * s = kdata_structure_init();
	kdata_structure_add(&s, &my_table);

	//init database
	kdata_init(DATABASE, s, DSERVICE_LOCAL, NULL, NULL, NULL);

	//add new item
	char uuid[37];
	kdata_add(DATABASE, my_table.tablename, &uuid, add_callback);
	printf("UUID: %s\n", uuid);
	
	printf("Done. press any key to exit\n");
	getchar();
	return 0;
}


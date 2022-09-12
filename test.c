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

//columns
const kdata_column ID = {
	.type = DTYPE_INT,
	.key = "ID"
};

const kdata_column NAME = {
	.type = DTYPE_TEXT,
	.key = "NAME"
};

const kdata_column DATE = {
	.type = DTYPE_INT,
	.key = "DATE"
};

//table
kdata_s * my_table() {
	kdata_s * table = malloc(sizeof(kdata_s));
}

int main(int argc, char *argv[])
{
	printf("Starting KDATA test...\n");
	
	printf("Done. press any key to exit\n");
	getchar();
	return 0;
}


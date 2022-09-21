/**
 * File              : list.h
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 23.03.2022
 * Last Modified Date: 06.09.2022
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */

/*
 * Library to manage lists
 * struct list_t * list_new() - create new list
 * void list_append(struct list_t * list, void * item) - append item to list 
 * int list_size(struct list_t * list) - return size of list
 * void * list_at(struct list_t * list, int i) - return item at position i
 * list_for_each - iterete the list
 */

#ifndef k_lib_list_h__
#define k_lib_list_h__

#ifdef __cplusplus
extern "C"{
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

struct list_t {
	struct list_t *next;
	void *data;
};

//allocate and init new LIST
#define list_new()\
({\
	struct list_t * const ___list = malloc(sizeof(struct list_t));\
	if(!___list) {perror("list malloc"); exit(EXIT_FAILURE);}\
	___list->next = NULL; ___list->data = NULL;\
   	___list;\
})

//free memory of LIST
#define list_free(list) \
({\
	struct list_t * ___ptr = list; \
	while(___ptr) { \
		struct list_t * const ___next_list = ___ptr->next; \
		free(___ptr); \
		___ptr = ___next_list; \
	} \
})

//prepend list item
#define list_prepend(list, item) \
({ \
	struct list_t * const ___next_list = list; \
	struct list_t * const ___new_list = list_new(); \
	___new_list->next = ___next_list; \
	___new_list->data = item; \
	list = ___new_list; \
	list; \
})
#define list_pop(list, item) list_prepend(list, item)

//append list item
#define list_append(list, item) \
({ \
	struct list_t * ___ptr = list; \
	if (___ptr->next) { \
		struct list_t * const ___new_list = list_new(); \
		___new_list->data = item; \
		while(___ptr->next->next) \
			___ptr = ___ptr->next; \
		struct list_t * const ___root_list = ___ptr->next; \
		___new_list->next = ___root_list; \
		___ptr->next = ___new_list; \
	} else { \
		list_prepend(list, item); \
	} \
	list; \
})
#define list_add(list, item) list_append(list, item)
#define list_push(list, item) list_append(list, item)

//return data value of item at index
#define list_at(list, index) \
({ \
	int ___i = 0; \
	void *___data = NULL; \
	struct list_t * ___ptr = list; \
	while(___ptr->next) {\
		if (___i == index) ___data = ___ptr->data;\
		___ptr = ___ptr->next; \
		___i++; \
	}\
___data; \
})
#define list_get(list, index) list_at(list, index)

//remove item at index, return data value
#define list_remove(list, index) \
({ \
	void *___data = NULL; \
	struct list_t * ___ptr = list; \
	if (index == 0 ){\
		___data = ___ptr->data;\
		list = ___ptr->next;\
		free(___ptr);\
	} else {\
		int ___i = 0; \
		struct list_t * ___prev = NULL; \
		while(___ptr->next) {\
			if (___i == index) {\
				___data = ___ptr->data;\
				___prev->next = ___ptr->next; \
				free(___ptr); \
				break; \
			}\
			___prev = ___ptr; \
			___ptr = ___ptr->next; \
			___i++; \
		}\
	}\
___data; \
})

//insert item at index
#define list_insert(list, item, index) \
({ \
	int ___isAdded = 0;\
	struct list_t * ___ptr = list; \
	struct list_t * ___prev = NULL; \
	if (index == 0 ){\
		list_prepend(list, item);\
		___isAdded = 1;\
	} else {\
		int ___i = 0; \
		while(___ptr->next) {\
			if (___i == index) {\
				struct list_t * ___new = list_new();\
				___new->data = item;\
				___prev->next = ___new; \
				___new->next = ___ptr; \
				___isAdded = 1;\
			}\
			___prev = ___ptr; \
			___ptr = ___ptr->next; \
			___i++; \
		}\
	}\
	if (!___isAdded) {\
		struct list_t * ___new = list_new();\
		___new->data = item;\
		___prev->next = ___new; \
		___new->next = ___ptr; \
	};\
})
#define list_set(list, item, index) list_insert(list, item, index)

//move item from index to index
#define list_move(list, from, to) \
({ \
	void * ___data = list_remove(list, from);\
	list_insert(list, ___data, to);\
})

//foreach item at list
#define list_for_each(list, item) \
	struct list_t * ___ptr = list; \
	void* item; \
	for (item = ___ptr->data; ___ptr->next; ___ptr=___ptr->next, item = ___ptr->data)

#define list_size(list) \
({ \
	int ___size = 0; \
	struct list_t * ___ptr = list; \
	while(___ptr->next) {\
		___ptr = ___ptr->next; \
		___size++; \
	} \
___size; \
})
#define list_count(list) list_size(list)

#ifdef __cplusplus
}
#endif

#endif //k_lib_list_h__

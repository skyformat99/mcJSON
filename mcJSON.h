/*
 * mcJSON, a modified version of cJSON, a simple JSON parser and generator.
 *
 * ISC License
 *
 * Copyright (C) 2015-2016 Max Bruckner (FSMaxB) <max at maxbruckner dot de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  This file incorporates work covered by the following license notice:
 *
 * |  Copyright (c) 2009 Dave Gamble
 * |
 * |  Permission is hereby granted, free of charge, to any person obtaining a copy
 * |  of this software and associated documentation files (the "Software"), to deal
 * |  in the Software without restriction, including without limitation the rights
 * |  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * |  copies of the Software, and to permit persons to whom the Software is
 * |  furnished to do so, subject to the following conditions:
 * |
 * |  The above copyright notice and this permission notice shall be included in
 * |  all copies or substantial portions of the Software.
 * |
 * |  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * |  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * |  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * |  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * |  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * |  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * |  THE SOFTWARE.
 */

#ifndef mcJSON__h
#define mcJSON__h

#include "buffer/buffer.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* mcJSON Types: */
typedef enum mcJSON_Type {
	mcJSON_False = (1 << 0),
	mcJSON_True = (1 << 1),
	mcJSON_NULL = (1 << 2),
	mcJSON_Number = (1 << 3),
	mcJSON_String = (1 << 4),
	mcJSON_Array = (1 << 5),
	mcJSON_Object = (1 << 6)
} mcJSON_Type;

/* alias buffer_t to mempool_t, the mempool_t type defines a contiguous
 * chunk of memory that is used for parsing a json into it. */
typedef buffer_t mempool_t;

/* The mcJSON structure: */
typedef struct mcJSON {
	struct mcJSON *next, *prev; /* next/prev allow you to walk array/object chains. Alternatively, use GetArrayItem/GetObjectItem */
	size_t length; /* the length of an array or an object */
	struct mcJSON *child; /* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */

	mcJSON_Type type; /* The type of the item, as above. */
	/* bitfield with two boolean variables */
	bool is_reference : 1;
	bool string_is_const : 1;

	buffer_t * valuestring; /* The item's string, if type==mcJSON_String */
	int valueint; /* The item's number, if type==mcJSON_Number */
	double valuedouble; /* The item's number, if type==mcJSON_Number */

	buffer_t * name; /* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
} mcJSON;

typedef struct mcJSON_Hooks {
      void *(*malloc_fn)(size_t sz);
      void (*free_fn)(void *ptr);
} mcJSON_Hooks;

/* Supply malloc, realloc and free functions to mcJSON */
extern void mcJSON_InitHooks(const mcJSON_Hooks * const hooks);


/* Supply a block of JSON, and this returns a mcJSON object you can interrogate. Call mcJSON_Delete when finished. */
extern mcJSON *mcJSON_Parse(buffer_t * const json);
/* Parse an object - create a new root, and populate.
 * This supports buffered parsing, a big chunk of memory
 * is allocated once and the json tree is parsed into it.
 * The size needs to be large enough otherwise allocation
 * will fail at some point. */
extern mcJSON *mcJSON_ParseWithBuffer(buffer_t * const json, mempool_t * const pool);
extern mcJSON *mcJSON_ParseBuffered(buffer_t *const json, const size_t bufer_length);
/* Render a mcJSON entity to text for transfer/storage. Free the char* when finished. */
extern buffer_t *mcJSON_Print(mcJSON * const item);
/* Render a mcJSON entity to text for transfer/storage without any formatting. Free the char* when finished. */
extern buffer_t *mcJSON_PrintUnformatted(mcJSON * const item);
/* Render a mcJSON entity to text using a buffered strategy. prebuffer is a guess at the final size. guessing well reduces reallocation. format = false gives unformatted, = true gives formatted */
extern buffer_t *mcJSON_PrintBuffered(mcJSON * const item, const size_t prebuffer, const bool format);
/* Delete a mcJSON entity and all subentities. */
extern void mcJSON_Delete(mcJSON * const c);

/* Retrieve item number "item" from array "array". Returns NULL if unsuccessful. */
extern mcJSON *mcJSON_GetArrayItem(const mcJSON *const array, size_t index);
/* Get item "string" from object. */
extern mcJSON *mcJSON_GetObjectItem(const mcJSON * const object, const buffer_t * const string);
/* check if a given Number is an Integer */
extern bool mcJSON_IsInteger(const mcJSON * const number);
/* check if a given json object is a boolean */
extern bool mcJSON_IsBoolean(const mcJSON * const json);

/* These calls create a mcJSON item of the appropriate type. */
extern mcJSON *mcJSON_CreateNull(mempool_t *pool);
extern mcJSON *mcJSON_CreateTrue(mempool_t *pool);
extern mcJSON *mcJSON_CreateFalse(mempool_t *pool);
extern mcJSON *mcJSON_CreateBool(const bool b, mempool_t *pool);
extern mcJSON *mcJSON_CreateNumber(const double num, mempool_t *pool);
extern mcJSON *mcJSON_CreateString(const buffer_t * const string, mempool_t *pool);
extern mcJSON *mcJSON_CreateHexString(const buffer_t * const binary, mempool_t *pool); /* create a hex string from binary input */
extern mcJSON *mcJSON_CreateArray(mempool_t *pool);
extern mcJSON *mcJSON_CreateObject(mempool_t *pool);

/* These utilities create an Array of count items. */
extern mcJSON *mcJSON_CreateIntArray(const int *numbers, const size_t count, mempool_t * const pool);
extern mcJSON *mcJSON_CreateDoubleArray(const double *numbers, const size_t count, mempool_t * const pool);
extern mcJSON *mcJSON_CreateStringArray(const buffer_t **strings, const size_t count, mempool_t * const pool);

/* Append item to the specified array/object. */
extern void mcJSON_AddItemToArray(mcJSON * const array, mcJSON * const item, mempool_t *pool);
extern void mcJSON_AddItemToObject(mcJSON * const object, const buffer_t * const string, mcJSON * const item, mempool_t * const pool);
extern void mcJSON_AddItemToObjectCS(mcJSON * const object, const buffer_t * const string, mcJSON * const item, mempool_t * const pool);	/* Use this when string is definitely const (i.e. a literal, or as good as), and will definitely survive the mcJSON object */
/* Append reference to item to the specified array/object. Use this when you want to add an existing mcJSON to a new mcJSON, but don't want to corrupt your existing mcJSON. */
extern void mcJSON_AddItemReferenceToArray(mcJSON * const array, const mcJSON * const item, mempool_t * const pool);
extern void mcJSON_AddItemReferenceToObject(mcJSON * const object, const buffer_t * const string, const mcJSON * const item, mempool_t * const pool);

/* Remove/Detatch items from Arrays/Objects. */
extern mcJSON *mcJSON_DetachItemFromArray(mcJSON * const array, const size_t index);
extern void mcJSON_DeleteItemFromArray(mcJSON *const array, const size_t index);
extern mcJSON *mcJSON_DetachItemFromObject(mcJSON * const object, const buffer_t * const string);
extern void mcJSON_DeleteItemFromObject(mcJSON * const object, const buffer_t * const string);

/* Update array items. */
extern void mcJSON_InsertItemInArray(mcJSON * const array, const size_t index, mcJSON * const newitem, mempool_t * const pool);	/* Shifts pre-existing items to the right. */
extern void mcJSON_ReplaceItemInArray(mcJSON * const array, const size_t index, mcJSON * const newitem, mempool_t * const pool);
extern void mcJSON_ReplaceItemInObject(mcJSON * const object, const buffer_t * const string, mcJSON * const newitem, mempool_t * const pool);

/* Duplicate a mcJSON item */
extern mcJSON *mcJSON_Duplicate(const mcJSON * const item, const int recurse, mempool_t * const pool);
/* Duplicate will create a new, identical mcJSON item to the one you pass, in new memory that will
need to be released. With recurse!=0, it will duplicate any children connected to the item.
The item->next and ->prev pointers are always zero on return from Duplicate. */

extern void mcJSON_Minify(buffer_t * const json);

/* Macros for creating things quickly. */
#define mcJSON_AddNullToObject(object, name, pool) mcJSON_AddItemToObject(object, name, mcJSON_CreateNull(pool), pool)
#define mcJSON_AddTrueToObject(object,name, pool) mcJSON_AddItemToObject(object, name, mcJSON_CreateTrue(pool), pool)
#define mcJSON_AddFalseToObject(object, name, pool) mcJSON_AddItemToObject(object, name, mcJSON_CreateFalse(pool), pool)
#define mcJSON_AddBoolToObject(object, name, b, pool) mcJSON_AddItemToObject(object, name, mcJSON_CreateBool(b, pool), pool)
#define mcJSON_AddNumberToObject(object, name, n, pool) mcJSON_AddItemToObject(object, name, mcJSON_CreateNumber(n, pool), pool)
#define mcJSON_AddStringToObject(object, name, s, pool) mcJSON_AddItemToObject(object, name, mcJSON_CreateString(s, pool), pool)

/* When assigning an integer value, it needs to be propagated to valuedouble too. */
#define mcJSON_SetIntValue(object,val)			((object)?(object)->valueint=(object)->valuedouble=(val):(val))
#define mcJSON_SetNumberValue(object,val)		((object)?(object)->valueint=(object)->valuedouble=(val):(val))

#ifdef __cplusplus
}
#endif

#endif

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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "mcJSON_Utils.h"

static int mcJSONUtils_strcasecmp(const char *s1, const char *s2) {
	if (s1 == NULL) {
		return (s1 == s2) ? 0 : 1;
	}
	if (s2 == NULL) {
		return 1;
	}
	for (; tolower(*s1) == tolower(*s2); ++s1, ++s2) {
		if (*s1 == 0) {
			return 0;
		}
	}
	return tolower(*(const unsigned char *)s1) - tolower(*(const unsigned char *)s2);
}

/* JSON Pointer implementation: */
static int mcJSONUtils_Pstrcasecmp(const char *a, const char *e) {
	if ((a == NULL) || (e == NULL)) {
		return (a == e) ? 0 : 1;
	}
	for (; *a && *e && (*e != '/'); a++,e++) {
		if (*e == '~') {
			if (!((e[1] == '0') && (*a == '~')) && !((e[1] == '1') && (*a=='/'))) {
				return 1;
			} else {
				e++;
			}
		} else if (tolower(*a) != tolower(*e))  {
			return 1;
		}
	}
	if (((*e != '\0') && (*e != '/')) != (*a != '\0')) {
		return 1;
	}
	return 0;
}

static int mcJSONUtils_PointerEncodedstrlen(const char *s) {
	int l = 0;
	for (; *s; s++,l++) {
		if ((*s == '~') || (*s == '/')) {
			l++;
		}
	}
	return l;
}

static void mcJSONUtils_PointerEncodedstrcpy(char *d, const char *s) {
	for (; *s; s++) {
		if (*s == '/') {
			*d++ = '~';
			*d++ = '1';
		} else if (*s == '~') {
			*d++ = '~';
			*d++ = '0';
		} else {
			*d++ = *s;
		}
	}
	*d = '\0';
}

char *mcJSONUtils_FindPointerFromObjectTo(mcJSON *object, mcJSON *target) {
	mcJSON_Type type = object->type;
	int c = 0;
	mcJSON *obj = NULL;

	if (object == target) {
		char *empty = malloc(1);
		empty[0] = '\0';
		return empty;
	}

	for (obj = object->child; obj; obj = obj->next, c++) {
		char *found = mcJSONUtils_FindPointerFromObjectTo(obj, target);
		if (found) {
			if (type == mcJSON_Array) {
				size_t length = strlen(found) + 23;
				char *ret = (char*)malloc(length);
				snprintf(ret, length, "/%d%s", c, found);
				free(found);
				return ret;
			} else if (type == mcJSON_Object) {
				char *ret = (char*)malloc(strlen(found) + mcJSONUtils_PointerEncodedstrlen((char*)obj->name->content) + 2);
				*ret = '/';
				mcJSONUtils_PointerEncodedstrcpy(ret + 1, (char*)obj->name->content);
				strcat(ret, found);
				free(found);
				return ret;
			}
			free(found);
			return NULL;
		}
	}
	return NULL;
}

mcJSON *mcJSONUtils_GetPointer(mcJSON *object, const char *pointer) {
	while ((*pointer++ == '/') && object) {
		if (object->type == mcJSON_Array) {
			int which=0;
			while ((*pointer >= '0') && (*pointer<='9')) {
				which = (10 * which) + *pointer++ - '0';
			}
			if (*pointer && (*pointer != '/')) {
				return NULL;
			}
			object = mcJSON_GetArrayItem(object, which);
		} else if (object->type == mcJSON_Object) {
			object = object->child;
			while (object && mcJSONUtils_Pstrcasecmp((char*)object->name->content, pointer)) { /* GetObjectItem. */
				object = object->next;
			}
			while (*pointer && (*pointer != '/')) {
				pointer++;
			}
		} else {
			return NULL;
		}
	}
	return object;
}

/* JSON Patch implementation. */
static void mcJSONUtils_InplaceDecodePointerString(char *string) {
	char *s2 = string;
	for (; *string; s2++, string++) {
		if (*string != '~') {
			*s2 = *string;
		} else if (*(++string) == '0') {
			*s2 = '~';
		} else {
			*s2 = '/';
		}
	}
	*s2 = '\0';
}

static mcJSON *mcJSONUtils_PatchDetach(mcJSON *object, const char *path) {
	buffer_t *parentptr = NULL;
	char *childptr = NULL;
	mcJSON *parent = NULL;
	mcJSON *ret = NULL;

	parentptr = buffer_create_on_heap(strlen(path) + 1, strlen(path) + 1);
	strcpy((char*)parentptr->content, path);
	childptr = strrchr((char*)parentptr->content, '/');
	if (childptr) {
		*childptr++ = '\0';
	} else {
		buffer_destroy_from_heap(parentptr);
		return ret;
	}
	parent = mcJSONUtils_GetPointer(object, (char*)parentptr->content);
	mcJSONUtils_InplaceDecodePointerString(childptr);

	if (parent == NULL) { /* Couldn't find object to remove child from. */
		ret = NULL;
	} else if (parent->type == mcJSON_Array) {
		ret = mcJSON_DetachItemFromArray(parent, atoi(childptr));
	} else if (parent->type == mcJSON_Object) {
		buffer_create_with_existing_array(childptr_buffer, (unsigned char*)childptr, strlen(childptr) + 1);
		ret = mcJSON_DetachItemFromObject(parent, childptr_buffer);
	}
	buffer_destroy_from_heap(parentptr);
	return ret;
}

static int mcJSONUtils_Compare(mcJSON *a, mcJSON *b) {
	if ((a == NULL) || (b == NULL)) { /* undefined */
		return -2;
	}
	if (a->type != b->type) { /* mismatched type. */
		return -1;
	}
	switch (a->type) {
		case mcJSON_Number:
			return ((a->valueint != b->valueint) || (a->valuedouble != b->valuedouble)) ? -2 : 0; /* numeric mismatch. */
		case mcJSON_String:
			return (strcmp((char*)a->valuestring->content, (char*)b->valuestring->content) != 0) ? -3 : 0; /* string mismatch. */
		case mcJSON_Array:
			for (a = a->child, b = b->child; a && b; a = a->next, b = b->next) {
				int err = mcJSONUtils_Compare(a, b);
				if (err) {
					return err;
				}
			}
			return (a || b) ? -4 : 0; /* array size mismatch. */
		case mcJSON_Object:
			mcJSONUtils_SortObject(a);
			mcJSONUtils_SortObject(b);
			a = a->child;
			b = b->child;
			while (a && b) {
				int err;
				if (mcJSONUtils_strcasecmp((char*)a->name, (char*)b->name)) { /* missing member */
					return -6;
				}
				err = mcJSONUtils_Compare(a, b);
				if (err) {
					return err;
				}
				a = a->next;
				b = b->next;
			}
			return (a || b) ? -5 : 0; /* object length mismatch */

		default:
			break;
	}
	return 0;
}

static int mcJSONUtils_ApplyPatch(mcJSON *object, mcJSON *patch) {
	mcJSON *op = NULL;
	mcJSON *path = NULL;
	mcJSON *value = NULL;
	mcJSON *parent = NULL;
	int opcode = 0;
	char *parentptr = NULL;
	char *childptr = NULL;

	buffer_create_from_string(op_buffer, "op");
	op = mcJSON_GetObjectItem(patch, op_buffer);
	buffer_create_from_string(path_buffer, "path");
	path = mcJSON_GetObjectItem(patch, path_buffer);
	if ((op == NULL) || (path == NULL)) { /* malformed patch. */
		return 2;
	}

	if (!strcmp((char*)op->valuestring->content,"add")) {
		opcode = 0;
	} else if (!strcmp((char*)op->valuestring->content, "remove")) {
		opcode = 1;
	} else if (!strcmp((char*)op->valuestring->content,"replace")) {
		opcode = 2;
	} else if (!strcmp((char*)op->valuestring->content,"move")) {
		opcode = 3;
	} else if (!strcmp((char*)op->valuestring->content, "copy")) {
		opcode = 4;
	} else if (!strcmp((char*)op->valuestring->content,"test")) {
		buffer_create_from_string(value_buffer, "value");
		return mcJSONUtils_Compare(mcJSONUtils_GetPointer(object, (char*)path->valuestring->content), mcJSON_GetObjectItem(patch, value_buffer));
	} else { /* unknown opcode. */
		return 3;
	}

	if ((opcode == 1) || (opcode == 2)) { /* Remove/Replace */
		mcJSON_Delete(mcJSONUtils_PatchDetach(object, (char*)path->valuestring->content)); /* Get rid of old. */
		if (opcode == 1) { /* For Remove, this is job done. */
			return 0;
		}
	}

	if ((opcode == 3) || (opcode == 4)) {/* Copy/Move uses "from". */
		buffer_create_from_string(from_buffer, "from");
		mcJSON *from = mcJSON_GetObjectItem(patch, from_buffer);
		if (from == NULL) { /* missing "from" for copy/move. */
			return 4;
		}

		if (opcode == 3) {
			value = mcJSONUtils_PatchDetach(object, (char*)from->valuestring->content);
		}
		if (opcode == 4) {
			value = mcJSONUtils_GetPointer(object, (char*)from->valuestring->content);
		}
		if (value == NULL) { /* missing "from" for copy/move. */
			return 5;
		}
		if (opcode == 4) {
			value = mcJSON_Duplicate(value, 1, NULL);
		}
		if (value == NULL) { /* out of memory for copy/move. */
			return 6;
		}
	} else { /* Add/Replace uses "value". */
		buffer_create_from_string(value_buffer, "value");
		value = mcJSON_GetObjectItem(patch, value_buffer);
		if (value == NULL) { /* missing "value" for add/replace. */
			return 7;
		}
		value = mcJSON_Duplicate(value, 1, NULL);
		if (value == NULL) { /* out of memory for add/replace. */
			return 8;
		}
	}

	/* Now, just add "value" to "path". */

	parentptr = malloc(strlen((char*)path->valuestring->content) + 1);
	strcpy(parentptr, (char*)path->valuestring->content);
	childptr = strrchr(parentptr, '/');
	if (childptr) {
		*childptr++ = '\0';
	} else {
		free(parentptr);
		mcJSON_Delete(value);
		return 10;
	}
	parent = mcJSONUtils_GetPointer(object, parentptr);
	mcJSONUtils_InplaceDecodePointerString(childptr);

	/* add, remove, replace, move, copy, test. */
	if (parent == NULL) { /* Couldn't find object to add to. */
		free(parentptr);
		mcJSON_Delete(value);
		return 9;
	} else if (parent->type == mcJSON_Array) {
		if (!strcmp(childptr, "-")) {
			mcJSON_AddItemToArray(parent,value, NULL);
		} else {
			mcJSON_InsertItemInArray(parent, atoi(childptr), value, NULL);
		}
	} else if (parent->type == mcJSON_Object) {
		buffer_create_with_existing_array(childptr_buffer, (unsigned char*)childptr, strlen(childptr) + 1);
		mcJSON_DeleteItemFromObject(parent, childptr_buffer);
		mcJSON_AddItemToObject(parent, childptr_buffer, value, NULL);
	} else {
		mcJSON_Delete(value);
	}
	free(parentptr);
	return 0;
}


int mcJSONUtils_ApplyPatches(mcJSON *object, mcJSON *patches) {
	int err;
	if ((patches == NULL) || (patches->type != mcJSON_Array)) { /* malformed patches. */
		return 1;
	}
	if (patches) {
		patches = patches->child;
	}
	while (patches) {
		if ((err = mcJSONUtils_ApplyPatch(object, patches))) {
			return err;
		}
		patches = patches->next;
	}
	return 0;
}

static void mcJSONUtils_GeneratePatch(mcJSON *patches, const char *op, const char *path, const char *suffix, mcJSON *val) {
	mcJSON *patch = mcJSON_CreateObject(NULL);
	buffer_create_from_string(op_literal_buffer, "op");
	buffer_create_with_existing_array(op_buffer, (unsigned char*)op, strlen(op) + 1);
	mcJSON_AddItemToObject(patch, op_literal_buffer, mcJSON_CreateString(op_buffer, NULL), NULL);
	if (suffix) {
		size_t length = strlen(path) + mcJSONUtils_PointerEncodedstrlen(suffix) + 2;
		buffer_t *newpath = buffer_create_on_heap(length, length);
		mcJSONUtils_PointerEncodedstrcpy((char*)newpath->content + snprintf((char*)newpath->content, newpath->content_length, "%s/", path), suffix);
		buffer_create_from_string(path_buffer, "path");
		mcJSON_AddItemToObject(patch, path_buffer, mcJSON_CreateString(newpath, NULL), NULL);
		buffer_destroy_from_heap(newpath);
	} else {
		buffer_create_with_existing_array(path_buffer, (unsigned char*)path, strlen(path) + 1);
		buffer_create_from_string(path_literal_buffer, "path");
		mcJSON_AddItemToObject(patch, path_literal_buffer, mcJSON_CreateString(path_buffer, NULL), NULL);
	}
	if (val) {
		buffer_create_from_string(value_buffer, "value");
		mcJSON_AddItemToObject(patch, value_buffer, mcJSON_Duplicate(val, 1, NULL), NULL);
	}
	mcJSON_AddItemToArray(patches, patch, NULL);
}

void mcJSONUtils_AddPatchToArray(mcJSON *array, const char *op, const char *path, mcJSON *val) {
	mcJSONUtils_GeneratePatch(array, op, path, 0, val);
}

static void mcJSONUtils_CompareToPatch(mcJSON *patches, const char *path, mcJSON *from, mcJSON *to) {
	if (from->type != to->type) {
		mcJSONUtils_GeneratePatch(patches, "replace", path, 0, to);
		return;
	}

	switch (from->type) {
		case mcJSON_Number:
			if ((from->valueint != to->valueint) || (from->valuedouble != to->valuedouble)) {
				mcJSONUtils_GeneratePatch(patches, "replace", path, 0, to);
			}
			return;

		case mcJSON_String:
			if (strcmp((char*)from->valuestring->content, (char*)to->valuestring->content) != 0) {
				mcJSONUtils_GeneratePatch(patches, "replace", path, 0, to);
			}
			return;

		case mcJSON_Array: {
			int c;
			size_t length = strlen(path) + 23; /* Allow space for 64bit int. */
			char *newpath = (char*)malloc(length);
			for (c = 0, from = from->child, to = to->child; from && to; from = from->next, to = to->next, c++) {
				snprintf(newpath, length, "%s/%d", path, c);
				mcJSONUtils_CompareToPatch(patches, newpath, from, to);
			}
			for (;from;from = from->next, c++) {
				snprintf(newpath, length, "%d", c);
				mcJSONUtils_GeneratePatch(patches, "remove", path, newpath, 0);
			}
			for (; to; to = to->next, c++) {
				mcJSONUtils_GeneratePatch(patches, "add", path, "-", to);
			}
			free(newpath);
			return;
		}

		case mcJSON_Object: {
			mcJSON *a,*b;
			mcJSONUtils_SortObject(from);
			mcJSONUtils_SortObject(to);

			a = from->child;
			b = to->child;
			while (a || b) {
				int diff;
				if (a == NULL) {
					diff = 1;
				} else if (b == NULL) {
					diff = -1;
				} else {
					diff = mcJSONUtils_strcasecmp((char*)a->name->content, (char*)b->name->content);
				}
				if (diff == 0) {
					size_t length = strlen(path) + mcJSONUtils_PointerEncodedstrlen((char*)a->name->content) + 2;
					char *newpath = (char*)malloc(length);
					mcJSONUtils_PointerEncodedstrcpy(newpath + snprintf(newpath, length, "%s/", path), (char*)a->name->content);
					mcJSONUtils_CompareToPatch(patches, newpath, a, b);
					free(newpath);
					a = a->next;
					b = b->next;
				} else if (diff < 0) {
					mcJSONUtils_GeneratePatch(patches, "remove", path, (char*)a->name->content, 0);
					a = a->next;
				} else {
					mcJSONUtils_GeneratePatch(patches, "add", path, (char*)b->name->content, b);
					b = b->next;
				}
			}
			return;
		}

		default:
			break;
	}
}


mcJSON* mcJSONUtils_GeneratePatches(mcJSON *from, mcJSON *to) {
	mcJSON *patches = mcJSON_CreateArray(NULL);
	mcJSONUtils_CompareToPatch(patches, "", from, to);
	return patches;
}


static mcJSON *mcJSONUtils_SortList(mcJSON *list) {
	mcJSON *first = list;
	mcJSON *second = list;
	mcJSON *ptr = list;

	if ((list == NULL) || (list->next == NULL)) { /* One entry is sorted already. */
		return list;
	}

	while (ptr && ptr->next && (mcJSONUtils_strcasecmp((char*)ptr->name->content, (char*)ptr->next->name->content) < 0)) { /* Test for list sorted. */
		ptr = ptr->next;
	}
	if ((ptr == NULL) || (ptr->next == NULL)) { /* Leave sorted lists unmodified. */
		return list;
	}
	ptr = list;

	while (ptr) {
		second = second->next;
		ptr = ptr->next;
		if (ptr) {
			ptr = ptr->next; /* Walk two pointers to find the middle. */
		}
	}
	if (second && second->prev) {
		second->prev->next = NULL; /* Split the lists */
	}

	first = mcJSONUtils_SortList(first); /* Recursively sort the sub-lists. */
	second = mcJSONUtils_SortList(second);
	list = NULL;
	ptr = NULL;

	while (first && second) {/* Merge the sub-lists */
		if (mcJSONUtils_strcasecmp((char*)first->name->content, (char*)second->name->content) < 0) {
			if (list == NULL) {
				ptr = first;
				list = ptr;
			} else {
				ptr->next = first;
				first->prev = ptr;
				ptr = first;
			}
			first = first->next;
		} else {
			if (list == NULL) {
				ptr = second;
				list = ptr;
			} else {
				ptr->next = second;
				second->prev = ptr;
				ptr = second;
			}
			second = second->next;
		}
	}
	if (first) { /* Append any tails. */
		if (list == NULL) {
			return first;
		}
		ptr->next = first;
		first->prev = ptr;
	}
	if (second) {
		if (list == NULL) {
			return second;
		}
		ptr->next = second;
		second->prev = ptr;
	}

	return list;
}

void mcJSONUtils_SortObject(mcJSON *object) {
	object->child = mcJSONUtils_SortList(object->child);
}

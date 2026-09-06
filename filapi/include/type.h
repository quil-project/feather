/*
 * Original feather Codebase
 * Copyright (c) 2026-present Quil Project Authors
 *
 * Released under the MIT License.
 */

/* Aggregate type API for the frontend */

#ifndef FILAPI_TYPE_H
#define FILAPI_TYPE_H

#include "../../all.h"
#include "../../config.h"
#include <stdint.h>

/* struct builder: flat structs; returns typ[] index (stable across grows) */
typedef struct IlType IlType; /* opaque; defined in type.c */

IlType *il_type_begin(const char *name);
void il_type_add_b(IlType *t, uint64_t n);
void il_type_add_h(IlType *t, uint64_t n);
void il_type_add_w(IlType *t, uint64_t n);
void il_type_add_l(IlType *t, uint64_t n);
void il_type_add_s(IlType *t, uint64_t n);
void il_type_add_d(IlType *t, uint64_t n);
void il_type_add_subtype(IlType *t, int idx, uint64_t n); /* nested :type */
int il_type_end(IlType *t);                               /* compute align/size, register, return index */

#endif // !FILAPI_TYPE_H

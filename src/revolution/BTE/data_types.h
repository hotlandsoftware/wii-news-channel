#ifndef DATA_TYPES_H
#define DATA_TYPES_H

/* Original source:
 * bluedroid <android.googlesource.com/platform/external/bluetooth/bluedroid>
 * gki/linux/data_types.h
 */

/******************************************************************************
 *
 *  Copyright (C) 1999-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/* Includes changes by muff1n1634 */

/*******************************************************************************
 * headers
 */

/* <stdint.h> types spelled out (no stdint.h in this tree) */

/*******************************************************************************
 * macros
 */

#define BCM_STRNCPY_S(x1, x2, x3, x4)	strncpy(x1, x3, x4)

/*******************************************************************************
 * types
 */

#ifdef __cplusplus
	extern "C" {
#endif

typedef signed char INT8;
typedef signed short INT16;
typedef signed long INT32;

typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned long UINT32;

typedef unsigned char BOOLEAN;

#ifndef TRUE
# define TRUE	1
#endif

#ifndef FALSE
# define FALSE	0
#endif

#ifdef __cplusplus
	}
#endif

#endif // DATA_TYPES_H

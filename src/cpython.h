/**
 * collectd - src/cpython.h
 * Copyright (C) 2009  Sven Trenkel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Sven Trenkel <collectd at semidefinite.de>  
 **/

/* These two macros are basicly Py_BEGIN_ALLOW_THREADS and Py_BEGIN_ALLOW_THREADS
 * from the other direction. If a Python thread calls a C function
 * Py_BEGIN_ALLOW_THREADS is used to allow other python threads to run because
 * we don't intend to call any Python functions.
 *
 * These two macros are used whenever a C thread intends to call some Python
 * function, usually because some registered callback was triggered.
 * Just like Py_BEGIN_ALLOW_THREADS it opens a block so these macros have to be
 * used in pairs. They aquire the GIL, create a new Python thread state and swap
 * the current thread state with the new one. This means this thread is now allowed
 * to execute Python code. */

#define CPY_LOCK_THREADS {\
	PyGILState_STATE gil_state;\
	gil_state = PyGILState_Ensure();

#define CPY_RETURN_FROM_THREADS \
	PyGILState_Release(gil_state);\
	return

#define CPY_RELEASE_THREADS \
	PyGILState_Release(gil_state);\
}

/* Python 2.4 has this macro, older versions do not. */
#ifndef Py_VISIT
#define Py_VISIT(o) do {\
	int _vret;\
	if ((o) != NULL) {\
		_vret = visit((o), arg);\
		if (_vret != 0)\
		return _vret;\
	}\
} while (0)
#endif

/* Python 2.4 has this macro, older versions do not. */
#ifndef Py_CLEAR
#define Py_CLEAR(o) do {\
	PyObject *tmp = o;\
	(o) = NULL;\
	Py_XDECREF(tmp);\
} while (0)
#endif

/* Python 2.4 has this macro, older versions do not. */
#ifndef Py_RETURN_NONE
# define Py_RETURN_NONE return Py_INCREF(Py_None), Py_None
#endif

typedef struct {
	PyObject_HEAD        /* No semicolon! */
	PyObject *parent;    /* Config */
	PyObject *key;       /* String */
	PyObject *values;    /* Sequence */
	PyObject *children;  /* Sequence */
} Config;

PyTypeObject ConfigType;

typedef struct {
	PyObject_HEAD        /* No semicolon! */
	double time;
	char host[DATA_MAX_NAME_LEN];
	char plugin[DATA_MAX_NAME_LEN];
	char plugin_instance[DATA_MAX_NAME_LEN];
	char type[DATA_MAX_NAME_LEN];
	char type_instance[DATA_MAX_NAME_LEN];
} PluginData;

PyTypeObject PluginDataType;

typedef struct {
	PluginData data;
	PyObject *values;    /* Sequence */
	int interval;
} Values;

PyTypeObject ValuesType;

typedef struct {
	PluginData data;
	int severity;
	char message[NOTIF_MAX_MSG_LEN];
} Notification;

PyTypeObject NotificationType;

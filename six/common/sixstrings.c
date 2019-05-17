// Unless explicitly stated otherwise all files in this repository are licensed
// under the Apache License Version 2.0.
// This product includes software developed at Datadog (https://www.datadoghq.com/).
// Copyright 2019 Datadog, Inc.
#include "sixstrings.h"

#include <six_types.h>

char *as_string(PyObject *object) {
    if (object == NULL) {
        return NULL;
    }

    char *retval = NULL;

#ifdef DATADOG_AGENT_THREE
    if (!PyUnicode_Check(object)) {
        return NULL;
    }

    PyObject *temp_bytes = PyUnicode_AsEncodedString(object, "UTF-8", "strict");
    if (temp_bytes == NULL) {
        return NULL;
    }

    retval = _strdup(PyBytes_AS_STRING(temp_bytes));
    Py_XDECREF(temp_bytes);
#else
    if (!PyString_Check(object)) {
        return NULL;
    }

    retval = _strdup(PyString_AS_STRING(object));
#endif
    return retval;
}

PyObject *json_module = NULL;
PyObject *json_loads = NULL;
PyObject *json_dumps = NULL;

void init_json() {
    PyGILState_STATE gstate = PyGILState_Ensure();

    char module_name[] = "json";
    json_module = PyImport_ImportModule(module_name);
    if (json_module == NULL) {
        PyErr_Clear();
        goto done;
    }

    char loads_name[] = "loads";
    json_loads = PyObject_GetAttrString(json_module, loads_name);
    if (json_loads == NULL) {
        PyErr_Clear();
        goto done;
    }

    char dumps_name[] = "dumps";
    json_dumps = PyObject_GetAttrString(json_module, dumps_name);
    if (json_dumps == NULL) {
        PyErr_Clear();
        goto done;
    }

done:
    PyGILState_Release(gstate);
}

PyObject *from_json(const char *data) {
    PyObject *retval = NULL;

    if (!data || !json_loads)
        Py_RETURN_NONE;

    return PyObject_CallFunction(json_loads, "s", data);
}

char *as_json(PyObject *object) {
    char *retval = NULL;
    PyObject *dumped = NULL;

    if (!json_dumps)
        return NULL;

    dumped = PyObject_CallFunctionObjArgs(json_dumps, object, NULL);
    retval = as_string(dumped);
    Py_XDECREF(dumped);
    return retval;
}

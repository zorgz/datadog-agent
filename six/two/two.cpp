// Unless explicitly stated otherwise all files in this repository are licensed
// under the Apache License Version 2.0.
// This product includes software developed at Datadog
// (https://www.datadoghq.com/).
// Copyright 2019 Datadog, Inc.
#include "two.h"

#include "constants.h"

#include <_util.h>
#include <aggregator.h>
#include <cgo_free.h>
#include <containers.h>
#include <datadog_agent.h>
#include <kubeutil.h>
#include <six_types.h>
#include <sixstrings.h>
#include <tagger.h>
#include <util.h>

#include <unistd.h>
#include <algorithm>
#include <sstream>
#include <iostream>

extern "C" DATADOG_AGENT_SIX_API Six *create(const char *pythonHome)
{
    return new Two(pythonHome);
}

extern "C" DATADOG_AGENT_SIX_API void destroy(Six *p)
{
    delete p;
}

Two::Two(const char *python_home)
    : Six()
    , _baseClass(NULL)
    , _pythonPaths()
{
    initPythonHome(python_home);
}

Two::~Two()
{
    PyEval_RestoreThread(_threadState);
    Py_XDECREF(_baseClass);
    Py_Finalize();
}

void Two::initPythonHome(const char *pythonHome)
{
    if (pythonHome != NULL && strlen(pythonHome) != 0) {
        _pythonHome = pythonHome;
    }

    Py_SetPythonHome(const_cast<char *>(_pythonHome));
}

void print_gil_state(std::string msg)
{
    std::cout << msg << ": gil state " << pthread_self() << " -> " << PyGILState_GetThisThreadState() << std::endl << std::flush;
}

bool Two::init()
{
    Py_Initialize();

    // In recent versions of Python3 this is called from Py_Initialize already,
    // for Python2 it has to be explicit.
    PyEval_InitThreads();

    // init custom builtins
    Py2_init_aggregator();
    Py2_init_datadog_agent();
    Py2_init_util();
    Py2_init__util();
    Py2_init_tagger();
    Py2_init_kubeutil();
    Py2_init_containers();

    // Set PYTHONPATH
    if (_pythonPaths.size()) {
        char pathchr[] = "path";
        PyObject *path = PySys_GetObject(pathchr); // borrowed
        for (PyPaths::iterator pit = _pythonPaths.begin(); pit != _pythonPaths.end(); ++pit) {
            PyObject *p = PyString_FromString((*pit).c_str());
            PyList_Append(path, p);
            Py_XDECREF(p);
        }
    }

    // import the base class
    _baseClass = _importFrom("datadog_checks.checks", "AgentCheck");

    // save tread state and release the GIL
    _threadState = PyEval_SaveThread();

    return _baseClass != NULL;
}

bool Two::isInitialized() const
{
    return Py_IsInitialized();
}

py_info_t *Two::getPyInfo()
{
    print_gil_state("getPyInfo");
    PyObject *sys = NULL;
    PyObject *path = NULL;
    PyObject *str_path = NULL;

    py_info_t *info = (py_info_t *)malloc(sizeof(*info));
    if (!info) {
        setError("could not allocate a py_info_t struct");
        return NULL;
    }

    info->version = Py_GetVersion();
    info->path = NULL;

    sys = PyImport_ImportModule("sys");
    if (!sys) {
        setError("could not import module 'sys': " + _fetchPythonError());
        goto done;
    }

    path = PyObject_GetAttrString(sys, "path");
    if (!path) {
        setError("could not get 'sys.path': " + _fetchPythonError());
        goto done;
    }

    str_path = PyObject_Repr(path);
    if (!str_path) {
        setError("could not compute a string representation of 'sys.path': " + _fetchPythonError());
        goto done;
    }

    info->path = as_string(str_path);

done:
    Py_XDECREF(sys);
    Py_XDECREF(path);
    Py_XDECREF(str_path);
    return info;
}

bool Two::runSimpleString(const char *code) const
{
    return PyRun_SimpleString(code) == 0;
}

bool Two::addPythonPath(const char *path)
{
    print_gil_state("addPythonPath");
    if (std::find(_pythonPaths.begin(), _pythonPaths.end(), path) == _pythonPaths.end()) {
        _pythonPaths.push_back(path);
        return true;
    }
    return false;
}

six_gilstate_t Two::GILEnsure()
{
    print_gil_state("GILEnsure");
    PyGILState_STATE state = PyGILState_Ensure();
    print_gil_state("GILEnsure done");
    if (state == PyGILState_LOCKED) {
        return DATADOG_AGENT_SIX_GIL_LOCKED;
    }
    return DATADOG_AGENT_SIX_GIL_UNLOCKED;
}

void Two::GILRelease(six_gilstate_t state)
{
    print_gil_state("GILRelease");
    if (state == DATADOG_AGENT_SIX_GIL_LOCKED) {
        PyGILState_Release(PyGILState_LOCKED);
    } else {
        PyGILState_Release(PyGILState_UNLOCKED);
    }
    print_gil_state("GILRelease done");
}

// return new reference
PyObject *Two::_importFrom(const char *module, const char *name)
{
    print_gil_state("_importFrom");
    PyObject *obj_module = NULL;
    PyObject *obj_symbol = NULL;

    obj_module = PyImport_ImportModule(module);
    if (obj_module == NULL) {
        setError(_fetchPythonError());
        goto error;
    }

    obj_symbol = PyObject_GetAttrString(obj_module, name);
    if (obj_symbol == NULL) {
        setError(_fetchPythonError());
        goto error;
    }

    Py_XDECREF(obj_module);
    return obj_symbol;

error:
    Py_XDECREF(obj_module);
    Py_XDECREF(obj_symbol);
    return NULL;
}

SixPyObject *Two::getCheckClass(const char *module)
{
    print_gil_state("getCheckClass");
    PyObject *obj_module = NULL;
    PyObject *klass = NULL;

done:
    Py_XDECREF(obj_module);
    Py_XDECREF(klass);

    if (klass == NULL) {
        return NULL;
    }

    return reinterpret_cast<SixPyObject *>(klass);
}

bool Two::getClass(const char *module, SixPyObject *&pyModule, SixPyObject *&pyClass)
{
    print_gil_state("getClass");
    PyObject *obj_module = NULL;
    PyObject *obj_class = NULL;

    std::cout << "getClass import module " << module << std::endl << std::flush;
    obj_module = PyImport_ImportModule(module);
    std::cout << "getClass import module: " << obj_module << std::endl << std::flush;
    if (obj_module == NULL) {
        std::cout << "getClass collecting error" << std::endl << std::flush;
        std::ostringstream err;
        err << "unable to import module '" << module << "': " + _fetchPythonError();
        setError(err.str());
        return false;
    }

    std::cout << "getClass _findSubclassOf" << std::endl << std::flush;
    obj_class = _findSubclassOf(_baseClass, obj_module);
    if (obj_class == NULL) {
        std::ostringstream err;
        err << "unable to find a subclass of the base check in module '" << module << "': " << _fetchPythonError();
        setError(err.str());
        Py_XDECREF(obj_module);
        return false;
    }

    pyModule = reinterpret_cast<SixPyObject *>(obj_module);
    pyClass = reinterpret_cast<SixPyObject *>(obj_class);
    return true;
}

bool Two::getCheck(SixPyObject *py_class, const char *init_config_str, const char *instance_str,
                   const char *check_id_str, const char *check_name, const char *agent_config_str, SixPyObject *&check)
{
    print_gil_state("getCheck");
    PyObject *klass = reinterpret_cast<PyObject *>(py_class);
    PyObject *agent_config = NULL;
    PyObject *init_config = NULL;
    PyObject *instance = NULL;
    PyObject *instances = NULL;
    PyObject *py_check = NULL;
    PyObject *args = NULL;
    PyObject *kwargs = NULL;
    PyObject *check_id = NULL;
    PyObject *name = NULL;

    char load_config[] = "load_config";
    char format[] = "(s)"; // use parentheses to force Tuple creation

    // call `AgentCheck.load_config(init_config)`
    std::cout << "getCheck call method load_config init_config: " << klass << std::endl << std::flush;
    init_config = PyObject_CallMethod(klass, load_config, format, init_config_str);
    if (init_config == NULL) {
        std::cout << "getCheck call method load_config init_config: error" << std::endl << std::flush;
        setError("error parsing init_config: " + _fetchPythonError());
        goto done;
    }
    std::cout << "getCheck call method load_config: success" << std::endl << std::flush;
    // replace an empty init_config by  a empty dict
    if (init_config == Py_None) {
        Py_XDECREF(init_config);
        init_config = PyDict_New();
    } else if (!PyDict_Check(init_config)) {
        setError("error 'init_config' is not a dict");
        goto done;
    }

    // call `AgentCheck.load_config(instance)`
    std::cout << "getCheck call method load_config instance: success" << std::endl << std::flush;
    instance = PyObject_CallMethod(klass, load_config, format, instance_str);
    if (instance == NULL) {
        std::cout << "getCheck call method load_config instance: error" << std::endl << std::flush;
        setError("error parsing instance: " + _fetchPythonError());
        goto done;
    } else if (!PyDict_Check(instance)) {
        setError("error instance is not a dict");
        goto done;
    }

    instances = PyTuple_New(1);
    std::cout << "getCheck SetItem" << std::endl << std::flush;
    if (PyTuple_SetItem(instances, 0, instance) != 0) {
        setError("Could not create Tuple for instances: " + _fetchPythonError());
        goto done;
    }

    // create `args` and `kwargs` to invoke `AgentCheck` constructor
    std::cout << "getCheck create dict" << std::endl << std::flush;
    args = PyTuple_New(0);
    kwargs = PyDict_New();
    std::cout << "getCheck PyString_FromString" << std::endl << std::flush;
    name = PyString_FromString(check_name);
    std::cout << "getCheck dict set" << std::endl << std::flush;
    PyDict_SetItemString(kwargs, "name", name);
    PyDict_SetItemString(kwargs, "init_config", init_config);
    PyDict_SetItemString(kwargs, "instances", instances);

    if (agent_config_str != NULL) {
        std::cout << "getCheck load_config agent_config" << std::endl << std::flush;
        agent_config = PyObject_CallMethod(klass, load_config, format, agent_config_str);
        if (agent_config == NULL) {
            std::cout << "getCheck load_config agent_config: error" << std::endl << std::flush;
            setError("error parsing agent_config: " + _fetchPythonError());
            goto done;
        } else if (!PyDict_Check(agent_config)) {
            setError("error agent_config is not a dict");
            goto done;
        }

        std::cout << "getCheck set config dict agent_config" << std::endl << std::flush;
        PyDict_SetItemString(kwargs, "agentConfig", agent_config);
    } else {
        std::cout << "getCheck NO agent_config" << std::endl << std::flush;
    }

    // call `AgentCheck` constructor
    std::cout << "getCheck Call AgentCheck constructor" << klass << " "  << args << " " << kwargs << std::endl << std::flush;
    py_check = PyObject_Call(klass, args, kwargs);
    if (py_check == NULL) {
        std::cout << "getCheck Call AgentCheck constructor: error" << std::endl << std::flush;
        setError(_fetchPythonError());
        goto done;
    }

    if (check_id_str != NULL && strlen(check_id_str) != 0) {
        std::cout << "getCheck setting check_id PyString_FromString" << std::endl << std::flush;
        check_id = PyString_FromString(check_id_str);
        if (check_id == NULL) {
            std::cout << "getCheck setting check_id PyString_FromString: error" << std::endl << std::flush;
            std::ostringstream err;
            err << "error could not set check_id: " << check_id_str;
            setError(err.str());
            Py_XDECREF(py_check);
            py_check = NULL;
            goto done;
        }

        std::cout << "getCheck setting check_id SetAttrString" << std::endl << std::flush;
        if (PyObject_SetAttrString(py_check, "check_id", check_id) != 0) {
            std::cout << "getCheck setting check_id SetAttrString: error" << std::endl << std::flush;
            setError("error could not set 'check_id' attr: " + _fetchPythonError());
            Py_XDECREF(py_check);
            py_check = NULL;
            goto done;
        }
    }

done:
    Py_XDECREF(name);
    Py_XDECREF(check_id);
    Py_XDECREF(init_config);
    Py_XDECREF(instance);
    Py_XDECREF(agent_config);
    Py_XDECREF(args);
    Py_XDECREF(kwargs);

    if (py_check == NULL) {
    std::cout << "getCheck Done error" << std::endl << std::flush;
        return false;
    }

    std::cout << "getCheck Done success" << std::endl << std::flush;
    check = reinterpret_cast<SixPyObject *>(py_check);
    return true;
}

PyObject *Two::_findSubclassOf(PyObject *base, PyObject *module)
{
    print_gil_state("_findSubclassOf");
    std::cout << "_findSubclassOf base" << std::endl << std::flush;
    // baseClass is not a Class type
    if (base == NULL || !PyType_Check(base)) {
        setError("base class is not of type 'Class'");
        return NULL;
    }

    std::cout << "_findSubclassOf check module" << std::endl << std::flush;
    // baseClass is not a Class type
    // module is not a Module object
    if (module == NULL || !PyModule_Check(module)) {
        setError("module is not of type 'Module'");
        return NULL;
    }

    std::cout << "_findSubclassOf dir" << std::endl << std::flush;
    // baseClass is not a Class type
    PyObject *dir = PyObject_Dir(module);
    if (dir == NULL) {
        setError("there was an error calling dir() on module object");
        return NULL;
    }

    PyObject *klass = NULL;
    for (int i = 0; i < PyList_GET_SIZE(dir); i++) {
        std::cout << "_findSubclassOf dir [" << i << "]" << std::endl << std::flush;
        // get symbol name
        char *symbol_name = NULL;
        PyObject *symbol = PyList_GetItem(dir, i);
        if (symbol == NULL) {
            continue;
        }

        // get symbol instance. It's a new ref but in case of success we don't
        // DecRef since we return it and the caller
        // will be owner
        symbol_name = PyString_AsString(symbol);
        std::cout << "_findSubclassOf dir [" << i << "]: " << symbol_name << std::endl << std::flush;
        klass = PyObject_GetAttrString(module, symbol_name);
        if (klass == NULL) {
            continue;
        }

        std::cout << "_findSubclassOf dir [" << i << "]: check klass" << std::endl << std::flush;
        // not a class, ignore
        if (!PyType_Check(klass)) {
            Py_XDECREF(klass);
            continue;
        }

        // this is an unrelated class, ignore
        std::cout << "_findSubclassOf dir [" << i << "]: check subtype" << std::endl << std::flush;
        if (!PyType_IsSubtype((PyTypeObject *)klass, (PyTypeObject *)base)) {
            Py_XDECREF(klass);
            continue;
        }

        std::cout << "_findSubclassOf dir [" << i << "]: check compare" << std::endl << std::flush;
        // `klass` is actually `base` itself, ignore
        if (PyObject_RichCompareBool(klass, base, Py_EQ)) {
            Py_XDECREF(klass);
            continue;
        }

        // does `klass` have subclasses?
        char func_name[] = "__subclasses__";
        std::cout << "_findSubclassOf dir [" << i << "]: call subclasses" << std::endl << std::flush;
        PyObject *children = PyObject_CallMethod(klass, func_name, NULL);
        if (children == NULL) {
            Py_XDECREF(klass);
            continue;
        }

        // how many?
        std::cout << "_findSubclassOf dir [" << i << "]: check get size children" << std::endl << std::flush;
        int children_count = PyList_GET_SIZE(children);
        Py_XDECREF(children);

        // Agent integrations are supposed to have no subclasses
        std::cout << "_findSubclassOf dir [" << i << "]: check count" << std::endl << std::flush;
        if (children_count > 0) {
            Py_XDECREF(klass);
            continue;
        }

        // got it, return the check class
        goto done;
    }

    setError("cannot find a subclass");
    klass = NULL;

done:
    std::cout << "_findSubclassOf finish " << klass << std::endl << std::flush;
    Py_XDECREF(dir);
    return klass;
}

const char *Two::runCheck(SixPyObject *check)
{
    print_gil_state("runCheck");
    if (check == NULL) {
        return NULL;
    }

    PyObject *py_check = reinterpret_cast<PyObject *>(check);

    // result will be eventually returned as a copy and the corresponding Python
    // string decref'ed, caller will be responsible for memory deallocation.
    char *ret, *ret_copy = NULL;
    char run[] = "run";
    PyObject *result = NULL;

    result = PyObject_CallMethod(py_check, run, NULL);
    if (result == NULL) {
        setError("error invoking 'run' method: " + _fetchPythonError());
        goto done;
    }

    // `ret` points to the Python string internal storage and will be eventually
    // deallocated along with the corresponding Python object.
    ret = PyString_AsString(result);
    if (ret == NULL) {
        setError("error converting result to string: " + _fetchPythonError());
        goto done;
    }

    ret_copy = _strdup(ret);

done:
    Py_XDECREF(result);
    return ret_copy;
}

char **Two::getCheckWarnings(SixPyObject *check)
{
    print_gil_state("getCheckWarnings");
    if (check == NULL)
        return NULL;
    PyObject *py_check = reinterpret_cast<PyObject *>(check);

    char func_name[] = "get_warnings";
    PyObject *warns_list = PyObject_CallMethod(py_check, func_name, NULL);
    if (warns_list == NULL) {
        setError("error invoking 'get_warnings' method: " + _fetchPythonError());
        return NULL;
    }

    Py_ssize_t numWarnings = PyList_Size(warns_list);
    char **warnings = (char **)malloc(sizeof(*warnings) * (numWarnings + 1));
    if (!warnings) {
        Py_XDECREF(warns_list);
        setError("could not allocate memory to get warnings: ");
        return NULL;
    }
    warnings[numWarnings] = NULL;

    for (Py_ssize_t idx = 0; idx < numWarnings; idx++) {
        PyObject *warn = PyList_GetItem(warns_list, idx); // borrowed ref
        warnings[idx] = as_string(warn);
    }
    Py_XDECREF(warns_list);
    return warnings;
}

std::string Two::_fetchPythonError()
{
    print_gil_state("_fetchPythonError");
    std::string ret_val = "";

    if (PyErr_Occurred() == NULL) {
        return ret_val;
    }

    PyObject *ptype = NULL;
    PyObject *pvalue = NULL;
    PyObject *ptraceback = NULL;

    // Fetch error and make sure exception values are normalized, as per python C
    // API docs.
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);

    // There's a traceback, try to format it nicely
    if (ptraceback != NULL) {
        PyObject *traceback = PyImport_ImportModule("traceback");
        if (traceback != NULL) {
            char fname[] = "format_exception";
            PyObject *format_exception = PyObject_GetAttrString(traceback, fname);
            if (format_exception != NULL) {
                PyObject *fmt_exc = PyObject_CallFunctionObjArgs(format_exception, ptype, pvalue, ptraceback, NULL);
                if (fmt_exc != NULL) {
                    // "format_exception" returns a list of strings (one per line)
                    for (int i = 0; i < PyList_Size(fmt_exc); i++) {
                        ret_val += PyString_AsString(PyList_GetItem(fmt_exc, i));
                    }
                }
                Py_XDECREF(fmt_exc);
                Py_XDECREF(format_exception);
            }
            Py_XDECREF(traceback);
        } else {
            // If we reach this point, there was an error while formatting the
            // exception
            ret_val = "can't format exception";
        }
    }
    // we sometimes do not get a traceback but an error in pvalue
    else if (pvalue != NULL) {
        PyObject *pvalue_obj = PyObject_Str(pvalue);
        if (pvalue_obj != NULL) {
            ret_val = PyString_AsString(pvalue_obj);
            Py_XDECREF(pvalue_obj);
        }
    } else if (ptype != NULL) {
        PyObject *ptype_obj = PyObject_Str(ptype);
        if (ptype_obj != NULL) {
            ret_val = PyString_AsString(ptype_obj);
            Py_XDECREF(ptype_obj);
        }
    }

    if (ret_val == "") {
        ret_val = "unknown error";
    }

    // clean up and return the string
    PyErr_Clear();
    Py_XDECREF(ptype);
    Py_XDECREF(pvalue);
    Py_XDECREF(ptraceback);
    return ret_val;
}

bool Two::getAttrString(SixPyObject *obj, const char *attributeName, char *&value) const
{
    print_gil_state("getAttrString");
    if (obj == NULL) {
        return false;
    }

    bool res = false;
    PyObject *py_attr = NULL;
    PyObject *py_obj = reinterpret_cast<PyObject *>(obj);

    py_attr = PyObject_GetAttrString(py_obj, attributeName);
    if (py_attr != NULL && PyString_Check(py_attr)) {
        value = as_string(py_attr);
        res = true;
    } else if (py_attr != NULL && !PyString_Check(py_attr)) {
        setError("error attribute " + std::string(attributeName) + " is has a different type than string");
        PyErr_Clear();
    } else {
        PyErr_Clear();
    }

    Py_XDECREF(py_attr);
    return res;
}

void Two::decref(SixPyObject *obj)
{
    print_gil_state("decref");
    Py_XDECREF(reinterpret_cast<PyObject *>(obj));
}

void Two::incref(SixPyObject *obj)
{
    print_gil_state("incref");
    Py_XINCREF(reinterpret_cast<SixPyObject *>(obj));
}

void Two::set_module_attr_string(char *module, char *attr, char *value)
{
    print_gil_state("set_module_attr_string");
    PyObject *py_module = PyImport_ImportModule(module);
    if (!py_module) {
        setError("error importing python '" + std::string(module) + "' module: " + _fetchPythonError());
        return;
    }

    PyObject *py_value = PyStringFromCString(value);
    if (PyObject_SetAttrString(py_module, attr, py_value) != 0)
        setError("error setting the '" + std::string(module) + "." + std::string(attr)
                 + "' attribute: " + _fetchPythonError());

    Py_XDECREF(py_module);
    Py_XDECREF(py_value);
}

void Two::setSubmitMetricCb(cb_submit_metric_t cb)
{
    _set_submit_metric_cb(cb);
}

void Two::setSubmitServiceCheckCb(cb_submit_service_check_t cb)
{
    _set_submit_service_check_cb(cb);
}

void Two::setSubmitEventCb(cb_submit_event_t cb)
{
    _set_submit_event_cb(cb);
}

void Two::setGetVersionCb(cb_get_version_t cb)
{
    _set_get_version_cb(cb);
}

void Two::setGetConfigCb(cb_get_config_t cb)
{
    _set_get_config_cb(cb);
}

void Two::setHeadersCb(cb_headers_t cb)
{
    _set_headers_cb(cb);
}

void Two::setGetHostnameCb(cb_get_hostname_t cb)
{
    _set_get_hostname_cb(cb);
}

void Two::setGetClusternameCb(cb_get_clustername_t cb)
{
    _set_get_clustername_cb(cb);
}

void Two::setLogCb(cb_log_t cb)
{
    _set_log_cb(cb);
}

void Two::setSetExternalTagsCb(cb_set_external_tags_t cb)
{
    _set_set_external_tags_cb(cb);
}

void Two::setSubprocessOutputCb(cb_get_subprocess_output_t cb)
{
    _set_get_subprocess_output_cb(cb);
}

void Two::setCGOFreeCb(cb_cgo_free_t cb)
{
    _set_cgo_free_cb(cb);
}

void Two::setTagsCb(cb_tags_t cb)
{
    _set_tags_cb(cb);
}

void Two::setGetConnectionInfoCb(cb_get_connection_info_t cb)
{
    _set_get_connection_info_cb(cb);
}

void Two::setIsExcludedCb(cb_is_excluded_t cb)
{
    _set_is_excluded_cb(cb);
}

// Python Helpers

// get_integration_list return a list of every datadog's wheels installed. The
// returned list must be free by calling free_integration_list.
char *Two::getIntegrationList()
{
    print_gil_state("getIntegrationList");
    PyObject *pyPackages = NULL;
    PyObject *pkgLister = NULL;
    PyObject *args = NULL;
    PyObject *packages = NULL;
    char *wheels = NULL;

    six_gilstate_t state = GILEnsure();

    pyPackages = PyImport_ImportModule("datadog_checks.base.utils.agent.packages");
    if (pyPackages == NULL) {
        setError("could not import datadog_checks.base.utils.agent.packages: " + _fetchPythonError());
        goto done;
    }

    pkgLister = PyObject_GetAttrString(pyPackages, "get_datadog_wheels");
    if (pyPackages == NULL) {
        setError("could not fetch get_datadog_wheels attr: " + _fetchPythonError());
        goto done;
    }

    args = PyTuple_New(0);
    packages = PyObject_Call(pkgLister, args, NULL);
    if (packages == NULL) {
        setError("error fetching wheels list: " + _fetchPythonError());
        goto done;
    }

    if (PyList_Check(packages) == 0) {
        setError("'get_datadog_wheels' did not return a list");
        goto done;
    }

    wheels = as_json(packages);

done:
    Py_XDECREF(pyPackages);
    Py_XDECREF(pkgLister);
    Py_XDECREF(args);
    Py_XDECREF(packages);
    GILRelease(state);

    return wheels;
}

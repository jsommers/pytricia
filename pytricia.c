#include <arpa/inet.h>
#include <Python.h>
#include "patricia.h"

typedef struct {
    PyObject_HEAD
    patricia_tree_t *m_tree;
    int m_family;
} PyTricia;

typedef struct {
    PyObject_HEAD
    patricia_tree_t *m_tree;
    patricia_node_t *m_Xnode;
    patricia_node_t *m_Xhead;
    patricia_node_t **m_Xstack;
    patricia_node_t **m_Xsp;
    patricia_node_t *m_Xrn;
    PyTricia *m_parent;
} PyTriciaIter;

#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 4
static PyObject *ipaddr_module = NULL;
static PyObject *ipaddr_base = NULL;
static PyObject *ipnet_base = NULL;
static int _ipaddr_isset = 0;
#endif

#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 4
static void _set_ipaddr_refs(void) {
    ipaddr_module = ipaddr_base = ipnet_base = NULL;
    if (_ipaddr_isset) {
        return;
    }
    _ipaddr_isset = 1;
    // 3.4 introduced ipaddress module.  get some static
    // references to the module and a base class for type
    // checking arguments to various methods.
    ipaddr_module = PyImport_ImportModule("ipaddress");
    if (ipaddr_module != NULL) {
        ipaddr_base = PyObject_GetAttrString(ipaddr_module, "_BaseAddress");
        ipnet_base = PyObject_GetAttrString(ipaddr_module, "_BaseNetwork");
        if (ipaddr_base == NULL && ipnet_base == NULL) {
            Py_DECREF(ipaddr_module);
            ipaddr_module = NULL;
        }
    }
}
#endif

static prefix_t * 
_prefix_convert(int family, const char *addr) {
    int prefixlen = -1;
    char addrcopy[128];

    if (strlen(addr) < 4) {
        return NULL;
    } 

    strncpy(addrcopy, addr, 128);
    char *slash = strchr(addrcopy, '/');
    if (slash != NULL) {
        *slash = '\0';
        slash++;
        if (strlen(slash) > 0) {
            prefixlen = (int)strtol(slash, NULL, 10);
        }
    }

    // if family isn't set, infer it
    if (family == 0) {
        if (strchr(addrcopy, ':')) {
            family = AF_INET6;
        } else {
            family = AF_INET;
        }
    }

    if (family == AF_INET) {
        if (prefixlen == -1 || prefixlen < 0 || prefixlen > 32) {
            prefixlen = 32;
        }
        struct in_addr sin;
        if (inet_pton(AF_INET, addrcopy, &sin) != 1) {
            return NULL;
        }
        return New_Prefix(AF_INET, &sin, prefixlen);
    } else if (family == AF_INET6) {
        if (prefixlen == -1 || prefixlen < 0 || prefixlen > 128) {
            prefixlen = 128;
        }

        struct in6_addr sin6;
        if (inet_pton(AF_INET6, addrcopy, &sin6) != 1) {
            return NULL;
        }
        return New_Prefix(AF_INET6, &sin6, prefixlen);
    } else {
        return NULL;
    }
}

static prefix_t *
_packed_addr_to_prefix(char *addrbuf, long len) {
    prefix_t *pfx_rv = NULL;
    if (len == 4) {
        pfx_rv = New_Prefix(AF_INET, addrbuf, 32);
    } else if (len == 16) {
        pfx_rv = New_Prefix(AF_INET6, addrbuf, 128);
    } else {
        PyErr_SetString(PyExc_ValueError, "Address bytes must be of length 4 or 16");
    }
    return pfx_rv;
}

#if PY_MAJOR_VERSION == 3
static prefix_t * 
_bytes_to_prefix(PyObject *key) {
    char *addrbuf = NULL;
    Py_ssize_t len = 0;
    if (PyBytes_AsStringAndSize(key, &addrbuf, &len) < 0) {
        PyErr_SetString(PyExc_ValueError, "Error decoding bytes");
        return NULL;
    }
    return _packed_addr_to_prefix(addrbuf, len);
}
#endif

static prefix_t *
_key_object_to_prefix(PyObject *key) {
    prefix_t *pfx_rv = NULL;
#if PY_MAJOR_VERSION == 3
#if PY_MINOR_VERSION >= 4
    if (!_ipaddr_isset) {
        _set_ipaddr_refs();
    }
#endif

    if (PyUnicode_Check(key)) {
        int rv = PyUnicode_READY(key); 
        if (rv < 0) { 
            PyErr_SetString(PyExc_ValueError, "Error parsing string prefix");
            return NULL;
        }
        char* temp = PyUnicode_AsUTF8(key);
        if (temp == NULL) {
            PyErr_SetString(PyExc_ValueError, "Error parsing string prefix");
            return NULL;
        }
        if (strchr(temp, '.') || strchr(temp, ':')) {
            pfx_rv = _prefix_convert(0, temp);
        } else {
            PyErr_SetString(PyExc_ValueError, "Invalid key type");
            return NULL;
        }
    } else if (PyLong_Check(key)) {
        unsigned long packed_addr = htonl(PyLong_AsUnsignedLong(key));
        pfx_rv = New_Prefix(AF_INET, &packed_addr, 32);
    } else if (PyBytes_Check(key)) {
        pfx_rv = _bytes_to_prefix(key);
    }
#if PY_MINOR_VERSION >= 4
    // do we have an IPv4/6Address or IPv4/6Network object (ipaddress
    // module added in Python 3.4
    else if (ipnet_base && PyObject_IsInstance(key, ipnet_base)) {
        PyObject *netaddr = PyObject_GetAttrString(key, "network_address");
        if (netaddr) {
            PyObject *packed = PyObject_GetAttrString(netaddr, "packed");
            if (packed && PyBytes_Check(packed)) {
                pfx_rv = _bytes_to_prefix(packed);
                PyObject *prefixlen = PyObject_GetAttrString(key, "prefixlen");
                if (prefixlen && PyLong_Check(prefixlen)) {
                    long bitlen = PyLong_AsLong(prefixlen);
                    pfx_rv->bitlen = bitlen;
                    Py_DECREF(prefixlen);
                }
                Py_DECREF(packed);
            } else {
                PyErr_SetString(PyExc_ValueError, "Error getting raw representation of IPNetwork");
            }
            Py_DECREF(netaddr);
        } else {
            PyErr_SetString(PyExc_ValueError, "Couldn't get network address from IPNetwork");
        }
    } else if (ipaddr_base && PyObject_IsInstance(key, ipaddr_base)) {
        PyObject *packed = PyObject_GetAttrString(key, "packed");
        if (packed && PyBytes_Check(packed)) {
            pfx_rv = _bytes_to_prefix(packed);
            Py_DECREF(packed);
        } else {
            PyErr_SetString(PyExc_ValueError, "Error getting raw representation of IPAddress");
        }
    } 
#endif
    else {
        PyErr_SetString(PyExc_ValueError, "Invalid key type");
    }
#else // python2
    if (PyString_Check(key)) {
        char* temp = PyString_AsString(key);
        Py_ssize_t slen = PyString_Size(key);
        if (strchr(temp, '.') || strchr(temp, ':')) {
            pfx_rv = _prefix_convert(0, temp);
        } else if (slen == 4 || slen == 16) {
            pfx_rv = _packed_addr_to_prefix(temp, slen);
        } else {
            PyErr_SetString(PyExc_ValueError, "Invalid key type");
        }
    } else if (PyLong_Check(key) || PyInt_Check(key)) {
        unsigned long packed_addr = htonl(PyInt_AsUnsignedLongMask(key));
        pfx_rv = New_Prefix(AF_INET, &packed_addr, 32);
    } else {
        PyErr_SetString(PyExc_ValueError, "Invalid key type");
    }
#endif
    return pfx_rv;
}

static void
pytricia_dealloc(PyTricia* self) {
    if (self) {
        Destroy_Patricia(self->m_tree, 0);
        Py_TYPE(self)->tp_free((PyObject*)self);
    }
}

static PyObject *
pytricia_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyTricia *self;

    self = (PyTricia*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->m_tree = NULL;
    }
    return (PyObject *)self;
}

static int
pytricia_init(PyTricia *self, PyObject *args, PyObject *kwds) {
    int prefixlen = 32;
    int family = AF_INET;
    if (!PyArg_ParseTuple(args, "|ii", &prefixlen, &family)) {
        self->m_tree = New_Patricia(1); // need to have *something* to dealloc
        PyErr_SetString(PyExc_ValueError, "Error parsing prefix length or address family");
        return -1;
    }

    if (prefixlen < 0 || prefixlen > PATRICIA_MAXBITS) {
        self->m_tree = New_Patricia(1); // need to have *something* to dealloc
        PyErr_SetString(PyExc_ValueError, "Invalid number of maximum bits; must be between 0 and 128, inclusive");
        return -1;
    } 

    if (!(family == AF_INET || family == AF_INET6)) {
        self->m_tree = New_Patricia(1); // need to have *something* to dealloc
        PyErr_SetString(PyExc_ValueError, "Invalid address family; must be AF_INET (2) or AF_INET6 (30)");
        return -1;
    }
    
    self->m_tree = New_Patricia(prefixlen);
    self->m_family = family;
    if (self->m_tree == NULL) {
        return -1;
    }
    return 0;
}

static Py_ssize_t 
pytricia_length(PyTricia *self) 
{
    patricia_node_t *node = NULL;
    Py_ssize_t count = 0;

    PATRICIA_WALK (self->m_tree->head, node) {
        count += 1;
    } PATRICIA_WALK_END;
    return count;
}

static PyObject* 
pytricia_subscript(PyTricia *self, PyObject *key) {
    prefix_t *subnet = _key_object_to_prefix(key);
    if (subnet == NULL) {
        PyErr_SetString(PyExc_ValueError, "Invalid prefix.");
        return NULL;
    }
    patricia_node_t* node = patricia_search_best(self->m_tree, subnet);
    Deref_Prefix(subnet);

    if (!node) {
        PyErr_SetString(PyExc_KeyError, "Prefix not found.");
        return NULL;
    }

    PyObject* data = (PyObject*)node->data;

    Py_INCREF(data);
    return data;
}

static int
pytricia_internal_delete(PyTricia *self, PyObject *key) {
    prefix_t *prefix = _key_object_to_prefix(key);
    if (prefix == NULL) {
        PyErr_SetString(PyExc_ValueError, "Invalid prefix.");
        return -1;
    }
    patricia_node_t* node = patricia_search_exact(self->m_tree, prefix);
    Deref_Prefix(prefix);

    if (!node) {
        PyErr_SetString(PyExc_KeyError, "Prefix doesn't exist.");
        return -1;
    }

    // decrement ref count on data referred to by key, if it exists
    PyObject* data = (PyObject*)node->data;
    Py_XDECREF(data);

    patricia_remove(self->m_tree, node);
    return 0;
}

static int 
_pytricia_assign_subscript_internal(PyTricia *self, PyObject *key, PyObject *value, long prefixlen) {
    if (!value) {
        return pytricia_internal_delete(self, key);
    }
    
    prefix_t *prefix = _key_object_to_prefix(key);
    if (!prefix) {
        return -1;
    }

    // if prefixlen > -1, it should override (possibly) parsed prefix len in key
    if (prefixlen != -1) {
        prefix->bitlen = prefixlen;
    }
    patricia_node_t *node = patricia_lookup(self->m_tree, prefix);
    Deref_Prefix(prefix);
    
    if (!node) {
        PyErr_SetString(PyExc_ValueError, "Error inserting into patricia tree");
        return -1;
    }

    Py_INCREF(value);
    node->data = value;

    return 0;
}

static int 
pytricia_assign_subscript(PyTricia *self, PyObject *key, PyObject *value) {
    return _pytricia_assign_subscript_internal(self, key, value, -1);
}

static PyObject*
pytricia_insert(PyTricia *self, PyObject *args) {
    PyObject *key = NULL;
    PyObject *value1 = NULL;
    PyObject *value2 = NULL;
    PyObject *rhs = NULL;

    if (!PyArg_ParseTuple(args, "O|OO", &key, &value1, &value2)) { 
        PyErr_SetString(PyExc_ValueError, "Invalid argument(s) to insert");
        return NULL;
    }

    if (value1) {
        long prefixlen = -1;
        if (value2 == NULL) {
            rhs = value1;
        } else {
            rhs = value2;
#if PY_MAJOR_VERSION == 3
            if (PyLong_Check(value1)) {
                prefixlen = PyLong_AsLong(value1);
            }
#else // python2
            if (PyLong_Check(value1) || PyInt_Check(value1)) {
                prefixlen  = PyInt_AsLong(value1);
            }
#endif
        }
        int rv = _pytricia_assign_subscript_internal(self, key, rhs, prefixlen); 
        if (rv == -1) {
            PyErr_SetString(PyExc_ValueError, "Invalid key.");
            return NULL;
        }
    } else {
        PyErr_SetString(PyExc_ValueError, "Missing argument(s) to insert");
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject*
pytricia_delitem(PyTricia *self, PyObject *args) {
    PyObject *key = NULL;
    if (!PyArg_ParseTuple(args, "O", &key)) {
        return NULL;
    }
    
    int rv = pytricia_internal_delete(self, key);
    if (rv < 0) {
        return NULL;
    } 
    Py_RETURN_NONE;
}

static PyObject *
pytricia_get(register PyTricia *obj, PyObject *args) {
    PyObject *key = NULL;
    PyObject *defvalue = NULL;

    if (!PyArg_ParseTuple(args, "O|O:get", &key, &defvalue)) {
        return NULL;
    }
    prefix_t *prefix = _key_object_to_prefix(key);
    if (!prefix) {
        PyErr_SetString(PyExc_ValueError, "Invalid prefix.");
        return NULL;
    }
    patricia_node_t* node = patricia_search_best(obj->m_tree, prefix);
    Deref_Prefix(prefix);

    if (!node) {
        if (defvalue) {
            Py_INCREF(defvalue);
            return defvalue;
        }
        Py_RETURN_NONE;
    }

    PyObject* data = (PyObject*)node->data;
    Py_INCREF(data);
    return data;
}

static PyObject *
pytricia_get_key(register PyTricia *obj, PyObject *args) {
    PyObject *key = NULL;

    if (!PyArg_ParseTuple(args, "O", &key)) {
        return NULL;
    }

    prefix_t *prefix = _key_object_to_prefix(key);
    if (!prefix) {
        PyErr_SetString(PyExc_ValueError, "Invalid prefix.");
        return NULL;
    }
    patricia_node_t* node = patricia_search_best(obj->m_tree, prefix);
    Deref_Prefix(prefix);

    if (!node) {
        Py_RETURN_NONE;
    }

    char buffer[64];
    prefix_toa2x(node->prefix, buffer, 1);
    PyObject *item = Py_BuildValue("s", buffer);
    if (!item) {
	   return NULL;
    }
    Py_INCREF(item);
    return item;
}

static int
pytricia_contains(PyTricia *self, PyObject *key) {
    prefix_t *prefix = _key_object_to_prefix(key);
    if (!prefix) {
        return 0;        
    }
    patricia_node_t* node = patricia_search_best(self->m_tree, prefix);
    Deref_Prefix(prefix);
    if (node) {
        return 1;
    }
    return 0;
}

static PyObject*
pytricia_has_key(PyTricia *self, PyObject *args) {
    PyObject *key = NULL;
    if (!PyArg_ParseTuple(args, "O", &key))
        return NULL;
    
    prefix_t *prefix = _key_object_to_prefix(key);
    if (!prefix) {
        PyErr_SetString(PyExc_ValueError, "Invalid prefix.");
        return NULL;
    }
    patricia_node_t* node = patricia_search_exact(self->m_tree, prefix);
    Deref_Prefix(prefix);
    if (node) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject* 
pytricia_keys(register PyTricia *self, PyObject *unused) {
    register PyObject *rvlist = PyList_New(0);
    if (!rvlist) {
        return NULL;
    }
    
    patricia_node_t *node = NULL;
    int err = 0;
    
    PATRICIA_WALK (self->m_tree->head, node) {
        char buffer[64];
        prefix_toa2x(node->prefix, buffer, 1);
        PyObject *item = Py_BuildValue("s", buffer);
        if (!item) {
            Py_DECREF(rvlist);
            return NULL;
        }
        err = PyList_Append(rvlist, item);
        Py_INCREF(item);
        if (err != 0) {
            Py_DECREF(rvlist);
            return NULL;
        }
    } PATRICIA_WALK_END;
    return rvlist;
}

static PyObject*
pytricia_children(register PyTricia *self, PyObject *args) {
    PyObject *key = NULL;

    if (!PyArg_ParseTuple(args, "O", &key)) {
        return NULL;
    }

    prefix_t *prefix = _key_object_to_prefix(key);
    if (!prefix) {
        PyErr_SetString(PyExc_ValueError, "Invalid prefix.");
        return NULL;
    }

    register PyObject *rvlist = PyList_New(0);
    if (!rvlist) {
        return NULL;
    }

    patricia_node_t* base_node = patricia_search_exact(self->m_tree, prefix);
    Deref_Prefix(prefix);
    if (!base_node) {
	   PyErr_SetString(PyExc_KeyError, "Prefix doesn't exist.");
	   Py_DECREF(rvlist);
	   return NULL;
    }
    patricia_node_t* node = NULL;
    int err = 0;

    PATRICIA_WALK (base_node, node) {
    	/* Discard first prefix (we want strict children) */
    	if (node != base_node) {
    	    char buffer[64];
    	    prefix_toa2x(node->prefix, buffer, 1);
    	    PyObject *item = Py_BuildValue("s", buffer);
    	    if (!item) {
    		    Py_DECREF(rvlist);
    		    return NULL;
    	    }
    	    err = PyList_Append(rvlist, item);
    	    Py_INCREF(item);
    	    if (err != 0) {
    		    Py_DECREF(rvlist);
    		    return NULL;
    	    }
	    }
    } PATRICIA_WALK_END;
    return rvlist;
}

static PyObject*
pytricia_parent(register PyTricia *self, PyObject *args) {
    PyObject *key = NULL;

    if (!PyArg_ParseTuple(args, "O", &key)) {
        return NULL;
    }

    prefix_t *prefix = _key_object_to_prefix(key);
    if (!prefix) {
        PyErr_SetString(PyExc_ValueError, "Invalid prefix.");
        return NULL;
    }

    patricia_node_t* node = patricia_search_exact(self->m_tree, prefix);
    Deref_Prefix(prefix);
    if (!node) {
	   PyErr_SetString(PyExc_KeyError, "Prefix doesn't exist.");
	   return NULL;
    }
    patricia_node_t* parent_node = patricia_search_best2(self->m_tree, node->prefix, 0);
    if (!parent_node) {
        Py_RETURN_NONE;
    }

    char buffer[64];
    prefix_toa2x(parent_node->prefix, buffer, 1);
    PyObject *item = Py_BuildValue("s", buffer);
    if (!item) {
	   return NULL;
    }
    Py_INCREF(item);
    return item;
}

static PyMappingMethods pytricia_as_mapping = {
    (lenfunc)pytricia_length,
    (binaryfunc)pytricia_subscript,
    (objobjargproc)pytricia_assign_subscript
};

static PySequenceMethods pytricia_as_sequence = {
       (lenfunc)pytricia_length,               /*sq_length*/
       0,              /*sq_concat*/
       0,                      /*sq_repeat*/
       0,                      /*sq_item*/
       0,                      /*sq_slice*/
       0,                      /*sq_ass_item*/
       0,                  /*sq_ass_slice*/
       (objobjproc)pytricia_contains,               /*sq_contains*/
       0,                  /*sq_inplace_concat*/
       0                   /*sq_inplace_repeat*/
};

// forward declaration
static PyObject*
pytricia_iter(register PyTricia *, PyObject *);


static PyMethodDef pytricia_methods[] = {
    {"has_key",   (PyCFunction)pytricia_has_key, METH_VARARGS, "has_key(prefix) -> boolean\nReturn true iff prefix is in tree.  Note that this method checks for an *exact* match with the prefix.\nUse the 'in' operator if you want to test whether a given address is contained within some prefix."},
    {"keys",   (PyCFunction)pytricia_keys, METH_NOARGS, "keys() -> list\nReturn a list of all prefixes in the tree."},
    {"get", (PyCFunction)pytricia_get, METH_VARARGS, "get(prefix, [default]) -> object\nReturn value associated with prefix."},
    {"get_key", (PyCFunction)pytricia_get_key, METH_VARARGS, "get_key(prefix) -> prefix\nReturn key associated with prefix (longest matching prefix)."},
    {"delete", (PyCFunction)pytricia_delitem, METH_VARARGS, "delete(prefix) -> \nDelete mapping associated with prefix.\n"},
    {"insert", (PyCFunction)pytricia_insert, METH_VARARGS, "insert(prefix, data) -> data\nCreate mapping between prefix and data in tree."},
    {"children", (PyCFunction)pytricia_children, METH_VARARGS, "children(prefix) -> list\nReturn a list of all prefixes that are more specific than the given prefix (the prefix must be present as an exact match)."},
    {"parent", (PyCFunction)pytricia_parent, METH_VARARGS, "parent(prefix) -> prefix\nReturn the immediate parent of the given prefix (the prefix must be present as an exact match)."},
    {NULL,              NULL}           /* sentinel */
};

static PyObject*
pytriciaiter_iter(PyTricia *self)
{
    Py_INCREF(self);
    return (PyObject*)self;
}

static PyObject*
pytriciaiter_next(PyTriciaIter *iter)
{
    while (1) {
        iter->m_Xnode = iter->m_Xrn;
        if (iter->m_Xnode) {
            // advance the iterator; copied from patricia.h macros
            if (iter->m_Xrn->l) { 
                if (iter->m_Xrn->r) { 
                    *(iter->m_Xsp)++ = iter->m_Xrn->r; 
                } 
                iter->m_Xrn = iter->m_Xrn->l; 
            } else if (iter->m_Xrn->r) { 
                iter->m_Xrn = iter->m_Xrn->r; 
            } else if (iter->m_Xsp != iter->m_Xstack) { 
                iter->m_Xrn = *(--iter->m_Xsp); 
            } else { 
                iter->m_Xrn = (patricia_node_t *) 0; 
            } 

            if (iter->m_Xnode->prefix) {
                /* build Python value to hand back */
                char buffer[64];
                prefix_toa2x(iter->m_Xnode->prefix, buffer, 1);
                PyObject *rv = Py_BuildValue("s", buffer);
                return rv;
            } 
        } else {
            PyErr_SetNone(PyExc_StopIteration);
            return NULL;
        }
    }
    // should NEVER get here...
    assert(NULL);
    return NULL;
}

static void
pytriciaiter_dealloc(PyTriciaIter *iterobj)
{
    if (iterobj->m_Xstack) {
        free(iterobj->m_Xstack);
    }
    Py_DECREF(iterobj->m_parent);
    Py_TYPE(iterobj)->tp_free((PyObject*)iterobj);    
}

static PyTypeObject PyTriciaType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "pytricia.PyTricia",       /*tp_name*/
    sizeof(PyTricia),          /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)pytricia_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    &pytricia_as_sequence,     /*tp_as_sequence*/
    &pytricia_as_mapping,      /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "PyTricia objects",        /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    (getiterfunc)pytricia_iter,	/* tp_iter */
    0,		               /* tp_iternext */
    pytricia_methods,          /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)pytricia_init,   /* tp_init */
    0,                         /* tp_alloc */
    pytricia_new,              /* tp_new */
};

static PyTypeObject PyTriciaIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "pytricia.PyTriciaIter",                /* tp_name */
    sizeof(PyTriciaIter),                   /* tp_basicsize */
    0,                                      /* tp_itemsize */
    /* methods */
    (destructor)pytriciaiter_dealloc,       /* tp_dealloc */
    0,                                      /* tp_print */
    0,                                      /* tp_getattr */
    0,                                      /* tp_setattr */
    0,                                      /* tp_compare */
    0,                                      /* tp_repr */
    0,                                      /* tp_as_number */
    0,                                      /* tp_as_sequence */
    0,                                      /* tp_as_mapping */
    0,                                      /* tp_hash */
    0,                                      /* tp_call */
    0,                                      /* tp_str */
    0,                                      /* tp_getattro */
    0,                                      /* tp_setattro */
    0,                                      /* tp_as_buffer */
#if PY_MAJOR_VERSION >= 3
    Py_TPFLAGS_DEFAULT,                     /* tp_flags */
#else
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER, /* tp_flags */
#endif
    "Internal PyTricia iter object",        /* tp_doc */
    0,                                      /* tp_traverse */
    0,                                      /* tp_clear */
    0,                                      /* tp_richcompare */
    0,                                      /* tp_weaklistoffset */
    (getiterfunc)pytriciaiter_iter,         /* tp_iter */
    (iternextfunc)pytriciaiter_next,        /* tp_iternext */
    0,                                      /* tp_methods */
};

static PyObject*
pytricia_iter(register PyTricia *self, PyObject *unused)
{
    PyTriciaIter *iterobj = PyObject_New(PyTriciaIter, &PyTriciaIterType);
    if (!iterobj) {
        return NULL;
    }

    if (!PyObject_Init((PyObject*)iterobj, &PyTriciaIterType)) {
        Py_XDECREF(iterobj);
        return NULL;
    }

    Py_INCREF(self);
    iterobj->m_parent = self;

    iterobj->m_tree = self->m_tree;
    iterobj->m_Xnode = NULL;
    iterobj->m_Xhead = iterobj->m_tree->head;
    iterobj->m_Xstack = (patricia_node_t**) malloc(sizeof(patricia_node_t*)*(PATRICIA_MAXBITS+1));
    if (!iterobj->m_Xstack) {
        Py_DECREF(iterobj->m_parent);
        Py_TYPE(iterobj)->tp_free((PyObject*)iterobj);
        return PyErr_NoMemory();
    }
 
    iterobj->m_Xsp = iterobj->m_Xstack;
    iterobj->m_Xrn = iterobj->m_Xhead;
    return (PyObject*)iterobj;
}


PyDoc_STRVAR(pytricia_doc,
"Yet another patricia tree module in Python.  But this one's better.\n\
");

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef pytricia_moduledef = {
    PyModuleDef_HEAD_INIT,
    "pytricia",       /* m_name */
    pytricia_doc,     /* m_doc */
    -1,               /* m_size */
    NULL,             /* m_methods */
    NULL,             /* m_reload */
    NULL,             /* m_traverse */
    NULL,             /* m_clear */
    NULL,             /* m_free */
};
#endif

#ifndef PyMODINIT_FUNC	/* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif

#if PY_MAJOR_VERSION >= 3
PyMODINIT_FUNC
PyInit_pytricia(void) 
#else
PyMODINIT_FUNC
initpytricia(void) 
#endif
{
    PyObject* m;

    if (PyType_Ready(&PyTriciaType) < 0)
#if PY_MAJOR_VERSION == 3
        return NULL;
#else
        return;
#endif

    PyTriciaIterType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyTriciaIterType) < 0)
#if PY_MAJOR_VERSION == 3
        return NULL;
#else
        return;
#endif

#if PY_MAJOR_VERSION == 3
    m = PyModule_Create(&pytricia_moduledef);
#else
    m = Py_InitModule3("pytricia", NULL, pytricia_doc);
#endif
    if (m == NULL)
#if PY_MAJOR_VERSION == 3 
      return NULL;
#else
      return;
#endif

    Py_INCREF(&PyTriciaType);
    Py_INCREF(&PyTriciaIterType);
    PyModule_AddObject(m, "PyTricia", (PyObject *)&PyTriciaType);

    // JS: don't add the PyTriciaIter object to the public interface.  users shouldn't be
    // able to create iterator objects w/o calling __iter__ on a pytricia object.

#if PY_MAJOR_VERSION == 3
    return m;
#endif
}


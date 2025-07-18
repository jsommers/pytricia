/*
 * This file is part of Pytricia.
 * Joel Sommers <jsommers@colgate.edu>
 * 
 * Pytricia is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Pytricia is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with Pytricia.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Python.h>
#include "patricia.h"

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#endif

typedef struct {
    PyObject_HEAD
    patricia_tree_t *m_tree;
    int m_family;
    u_short m_raw_output;
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

static int
_prefix_convert(int family, const char *addr, prefix_t* prefix) {
    int prefixlen = -1;
    char addrcopy[128];

    if (strlen(addr) < 4) {
        return 0;
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
            return 0;
        }
        return New_Prefix(AF_INET, &sin, prefixlen, prefix);
    } else if (family == AF_INET6) {
        if (prefixlen == -1 || prefixlen < 0 || prefixlen > 128) {
            prefixlen = 128;
        }

        struct in6_addr sin6;
        if (inet_pton(AF_INET6, addrcopy, &sin6) != 1) {
            return 0;
        }
        return New_Prefix(AF_INET6, &sin6, prefixlen, prefix);
    } else {
        return 0;
    }
}

static int
_packed_addr_to_prefix(char *addrbuf, long len, prefix_t* pfx_rv) {
    int ret_ok = 0;
    if (len == 4) {
        ret_ok = New_Prefix(AF_INET, addrbuf, 32, pfx_rv);
    } else if (len == 16) {
        ret_ok = New_Prefix(AF_INET6, addrbuf, 128, pfx_rv);
    } else {
        PyErr_SetString(PyExc_ValueError, "Address bytes must be of length 4 or 16");
    }
    return ret_ok;
}

#if PY_MAJOR_VERSION == 3
static int
_bytes_to_prefix(PyObject *key, prefix_t* pfx_rv) {
    char *addrbuf = NULL;
    Py_ssize_t len = 0;
    if (PyBytes_AsStringAndSize(key, &addrbuf, &len) < 0) {
        PyErr_SetString(PyExc_ValueError, "Error decoding bytes");
        return 0;
    }
    return _packed_addr_to_prefix(addrbuf, len, pfx_rv);
}
#endif

static int
_key_object_to_prefix(PyObject *key, prefix_t* pfx_rv) {
    int ret_ok = 0;
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
            return 0;
        }
        const char* temp = PyUnicode_AsUTF8(key);
        if (temp == 0) {
            PyErr_SetString(PyExc_ValueError, "Error parsing string prefix");
            return 0;
        }
        if (strchr(temp, '.') || strchr(temp, ':')) {
            ret_ok = _prefix_convert(0, temp, pfx_rv);
        } else {
            PyErr_SetString(PyExc_ValueError, "Invalid key type");
            return 0;
        }
    } else if (PyLong_Check(key)) {
        unsigned long packed_addr = htonl(PyLong_AsUnsignedLong(key));
        ret_ok = New_Prefix(AF_INET, &packed_addr, 32, pfx_rv);
    } else if (PyBytes_Check(key)) {
        ret_ok = _bytes_to_prefix(key, pfx_rv);
    } else if (PyTuple_Check(key)) {
        PyObject* value = PyTuple_GetItem(key, 0);
        PyObject* size = PyTuple_GetItem(key, 1);
        if (!PyBytes_Check(value)) {
            PyErr_SetString(PyExc_ValueError, "Invalid key tuple value type");
            return 0;
        }
        Py_ssize_t slen = PyBytes_Size(value);
        if (slen != 4 && slen != 16) {
            PyErr_SetString(PyExc_ValueError, "Invalid key tuple value");
            return 0;
        }
        ret_ok = _bytes_to_prefix(value, pfx_rv);
        if (ret_ok) {
	        if (!PyLong_Check(size)) {
	            PyErr_SetString(PyExc_ValueError, "Invalid key tuple size type");
	            return 0;
	        }
	        unsigned long bitlen = PyLong_AsUnsignedLong(size);
	        if (bitlen < pfx_rv->bitlen)
	            pfx_rv->bitlen = bitlen;
	    }
    }
#if PY_MINOR_VERSION >= 4
    // do we have an IPv4/6Address or IPv4/6Network object (ipaddress
    // module added in Python 3.4
    else if (ipnet_base && PyObject_IsInstance(key, ipnet_base)) {
        PyObject *netaddr = PyObject_GetAttrString(key, "network_address");
        if (netaddr) {
            PyObject *packed = PyObject_GetAttrString(netaddr, "packed");
            if (packed && PyBytes_Check(packed)) {
                ret_ok = _bytes_to_prefix(packed, pfx_rv);
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
            ret_ok = _bytes_to_prefix(packed, pfx_rv);
            Py_DECREF(packed);
        } else {
            PyErr_SetString(PyExc_ValueError, "Error getting raw representation of IPAddress");
        }
    } 
#endif
    else {
        PyErr_SetString(PyExc_ValueError, "Invalid key type");
    }
#else //python2
    if (PyString_Check(key)) {
        char* temp = PyString_AsString(key);
        Py_ssize_t slen = PyString_Size(key);
        if (strchr(temp, '.') || strchr(temp, ':')) {
            ret_ok = _prefix_convert(0, temp, pfx_rv);
        } else if (slen == 4 || slen == 16) {
            ret_ok = _packed_addr_to_prefix(temp, slen, pfx_rv);
        } else {
            PyErr_SetString(PyExc_ValueError, "Invalid key type");
        }
    } else if (PyLong_Check(key) || PyInt_Check(key)) {
        unsigned long packed_addr = htonl(PyInt_AsUnsignedLongMask(key));
        ret_ok = New_Prefix(AF_INET, &packed_addr, 32, pfx_rv);
    } else if (PyTuple_Check(key)) {
        PyObject* value = PyTuple_GetItem(key, 0);
        PyObject* size = PyTuple_GetItem(key, 1);
        if (!PyString_Check(value)) {
            PyErr_SetString(PyExc_ValueError, "Invalid key tuple value type");
            return 0;
        }
        Py_ssize_t slen = PyString_Size(value);
        if (slen != 4 && slen != 16) {
            PyErr_SetString(PyExc_ValueError, "Invalid key tuple value");
            return 0;
        }
        char* temp = PyString_AsString(value);
        ret_ok = _packed_addr_to_prefix(temp, slen, pfx_rv);
        if (pfx_rv) {
	        if (!PyLong_Check(size) && !PyInt_Check(size)) {
	            PyErr_SetString(PyExc_ValueError, "Invalid key tuple size type");
	            return 0;
	        }
	        unsigned long bitlen = PyInt_AsLong(size);
	        if (bitlen < pfx_rv->bitlen)
	            pfx_rv->bitlen = bitlen;
	    }
    } else {
        PyErr_SetString(PyExc_ValueError, "Invalid key type");
    }
#endif
    return ret_ok;
}

static PyObject *
_prefix_to_key_object(prefix_t* prefix, int raw_output) {
    if (raw_output) {
        int addr_size;
        char *addr = (char*) prefix_touchar(prefix);
        if (prefix->family == AF_INET6) {
            addr_size = 16;
        } else {
            addr_size = 4;
        }
        PyObject* value;
#if PY_MAJOR_VERSION == 3
        value = PyBytes_FromStringAndSize(addr, addr_size);
#else
        value = PyString_FromStringAndSize(addr, addr_size);
#endif
        PyObject* tuple;
        tuple = Py_BuildValue("(Oi)", value, prefix->bitlen);
        Py_XDECREF(value);
        return tuple;
    }
    char buffer[64];
    prefix_toa2x(prefix, buffer, 1);
    return Py_BuildValue("s", buffer);
}

static void
pytricia_xdecref(void *data) {
    Py_XDECREF((PyObject*)data);
}

static void
pytricia_dealloc(PyTricia* self) {
    if (self) {
        Destroy_Patricia(self->m_tree, pytricia_xdecref);
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
    PyObject* raw_output = NULL;
    if (!PyArg_ParseTuple(args, "|iiO", &prefixlen, &family, &raw_output)) {
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
    self->m_raw_output = raw_output && PyObject_IsTrue(raw_output);
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
    prefix_t prefix; memset(&prefix, 0, sizeof(prefix));
    int ret_ok = _key_object_to_prefix(key, &prefix);
    if (!ret_ok) {
        PyErr_SetString(PyExc_ValueError, "Invalid prefix.");
        return NULL;
    }
    patricia_node_t* node = patricia_search_best(self->m_tree, &prefix);

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
    if (self->m_tree->frozen) {
        PyErr_SetString(PyExc_ValueError, "can not modify a frozen pytricia!  Thaw?");
        return -1;
    }
    prefix_t prefix; memset(&prefix, 0, sizeof(prefix));

    int ret_ok = _key_object_to_prefix(key, &prefix);
    if (!ret_ok) {
        PyErr_SetString(PyExc_ValueError, "Invalid prefix.");
        return -1;
    }
    patricia_node_t* node = patricia_search_exact(self->m_tree, &prefix);

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

    if (self->m_tree->frozen) {
        PyErr_SetString(PyExc_ValueError, "can not modify a frozen pytricia!  Thaw?");
        return -1;
    }

    prefix_t prefix; memset(&prefix, 0, sizeof(prefix));
    int ret_ok = _key_object_to_prefix(key, &prefix);
    if (!ret_ok) {
        return -1;
    }

    // if prefixlen > -1, it should override (possibly) parsed prefix len in key
    if (prefixlen != -1) {
        prefix.bitlen = prefixlen;
    }
    patricia_node_t *node = patricia_lookup(self->m_tree, &prefix);
    
    if (!node) {
        PyErr_SetString(PyExc_ValueError, "Error inserting into patricia tree");
        return -1;
    }

    // node already existed, lower ref count on old data 
    if (node->data) {
        PyObject* data = (PyObject*)node->data;
        Py_DECREF(data);
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
    prefix_t prefix; memset(&prefix, 0, sizeof(prefix));
    int ret_ok = _key_object_to_prefix(key, &prefix);
    if (!ret_ok) {
        PyErr_SetString(PyExc_ValueError, "Invalid prefix.");
        return NULL;
    }
    patricia_node_t* node = patricia_search_best(obj->m_tree, &prefix);

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

    prefix_t prefix; memset(&prefix, 0, sizeof(prefix));
    int ret_ok = _key_object_to_prefix(key, &prefix);
    if (!ret_ok) {
        PyErr_SetString(PyExc_ValueError, "Invalid prefix.");
        return NULL;
    }
    patricia_node_t* node = patricia_search_best(obj->m_tree, &prefix);

    if (!node) {
        Py_RETURN_NONE;
    }

    return _prefix_to_key_object(&node->prefix, obj->m_raw_output);
}

static int
pytricia_contains(PyTricia *self, PyObject *key) {
    prefix_t prefix; memset(&prefix, 0, sizeof(prefix));
    int ret_ok = _key_object_to_prefix(key, &prefix);
    if (!ret_ok) {
        return -1;
    }
    patricia_node_t* node = patricia_search_best(self->m_tree, &prefix);
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
    prefix_t prefix; memset(&prefix, 0, sizeof(prefix));
    int ret_ok = _key_object_to_prefix(key, &prefix);
    if (!ret_ok) {
        PyErr_SetString(PyExc_ValueError, "Invalid prefix.");
        return NULL;
    }
    patricia_node_t* node = patricia_search_exact(self->m_tree, &prefix);
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
        PyObject *item = _prefix_to_key_object(&node->prefix, self->m_raw_output);
        if (!item) {
            Py_DECREF(rvlist);
            return NULL;
        }
        err = PyList_Append(rvlist, item);
        Py_DECREF(item);
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

    prefix_t prefix; memset(&prefix, 0, sizeof(prefix));
    int ret_ok = _key_object_to_prefix(key, &prefix);
    if (!ret_ok) {
        PyErr_SetString(PyExc_ValueError, "Invalid prefix.");
        return NULL;
    }

    register PyObject *rvlist = PyList_New(0);
    if (!rvlist) {
        return NULL;
    }

    patricia_node_t* base_node = patricia_search_exact(self->m_tree, &prefix);
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
            PyObject *item = _prefix_to_key_object(&node->prefix, self->m_raw_output);
            if (!item) {
                Py_DECREF(rvlist);
                return NULL;
            }
            err = PyList_Append(rvlist, item);
            Py_DECREF(item);
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

    prefix_t prefix; memset(&prefix, 0, sizeof(prefix));
    int ret_ok = _key_object_to_prefix(key, &prefix);
    if (!ret_ok) {
        PyErr_SetString(PyExc_ValueError, "Invalid prefix.");
        return NULL;
    }

    patricia_node_t* node = patricia_search_exact(self->m_tree, &prefix);
    if (!node) {
	   PyErr_SetString(PyExc_KeyError, "Prefix doesn't exist.");
	   return NULL;
    }
    patricia_node_t* parent_node = patricia_search_best2(self->m_tree, &node->prefix, 0);
    if (!parent_node) {
        Py_RETURN_NONE;
    }

    return _prefix_to_key_object(&parent_node->prefix, self->m_raw_output);
}

static PyObject*
pytricia_freeze(register PyTricia *self, PyObject *unused) {
    if (self->m_tree->frozen) {
        Py_RETURN_NONE;
    }

    patricia_node_t *node = NULL;

    // Get count of all nodes
    size_t count = 0;
    PATRICIA_WALK_ALL (self->m_tree->head, node) {
        count += 1;
    } PATRICIA_WALK_END;

    // allocate contiguous space for all nodes
    patricia_node_t* new_node = calloc(count, sizeof(patricia_node_t));

    // copy all nodes to new array and discard originals
    size_t idx = 0;
    patricia_node_t** free_list = calloc(count, sizeof(patricia_node_t*));
    PATRICIA_WALK_ALL (self->m_tree->head, node) {
        new_node[idx] = *node;
        if(node->l)
            node->l->parent = &(new_node[idx]);
        if(node->r)
            node->r->parent = &(new_node[idx]);
        if(node->parent == NULL) {
            assert (self->m_tree->head == node);
            self->m_tree->head = new_node;
        }
        else if (node->parent->r == node) {
            node->parent->r = &(new_node[idx]);
        }
        else {
            node->parent->l = &(new_node[idx]);
        }
        free_list[idx] = node;
        idx++;
    } PATRICIA_WALK_END;

    // clearing via free_list because otherise PATRICIA_WALK_END
    // will illegally reference items from node to determine next steps
    for(size_t i=0; i<count; i++) {
        free(free_list[i]);
    }
    free(free_list);

    // mark as frozen
    self->m_tree->frozen = 1;

    Py_RETURN_NONE;
}

static PyObject*
pytricia_thaw(register PyTricia *self, PyObject *unused) {
    if (!self->m_tree->frozen) {
        Py_RETURN_NONE;  // already thaw'd
    }
    if (self->m_tree->head == NULL) {
        self->m_tree->frozen = 0;
        Py_RETURN_NONE;
    }
    patricia_node_t* original_head = self->m_tree->head;

    // walk all nodes, allocating heap space for individual
    // nodes and re-linking
    patricia_node_t *node = NULL;
    PATRICIA_WALK_ALL (self->m_tree->head, node) {
        patricia_node_t* new_node = calloc(1, sizeof(patricia_node_t));
        *new_node = *node;
        if(node->l)
            node->l->parent = new_node;
        if(node->r)
            node->r->parent = new_node;
        if(node->parent == NULL) {
            assert (self->m_tree->head == node);
            self->m_tree->head = new_node;
        }
        else if (node->parent->r == node) {
            node->parent->r = new_node;
        }
        else {
            node->parent->l = new_node;
        }
    } PATRICIA_WALK_END;

    free(original_head);

    // mark as NOT frozen
    self->m_tree->frozen = 0;

    Py_RETURN_NONE;
}

// forward declaration
static PyTypeObject PyTriciaType;

static PyObject* pytricia_reduce(PyTricia *self, PyObject *Py_UNUSED(ignored)) {
    if (!self->m_tree->frozen) {
        PyErr_SetString(PyExc_RuntimeError, "pytri must be frozen before attempting to pickle!");
        return NULL;
    }

    PyObject *args = PyTuple_New(1);
    if (!args) {
      return NULL;
    }
    PyTuple_SET_ITEM(args, 0, PyLong_FromLong(self->m_tree->maxbits));

    // setup the extra dictionary info for setstate
    PyObject* dict = PyDict_New();
    PyObject* bytes = PyBytes_FromStringAndSize((const char*)self->m_tree, sizeof(patricia_tree_t));

    if (!dict || !bytes) {
	    PyErr_SetString(PyExc_MemoryError, "unable to allocate space for tri");
        Py_XDECREF(dict);
        Py_XDECREF(bytes);
        return NULL;
    }

    int ret_err = PyDict_SetItemString(dict, "tree", bytes);
    Py_DECREF(bytes);  // dictionary now owns the reference
    if(ret_err) {
        PyErr_SetString(PyExc_MemoryError, "error writing tree_t to dictionary");
        Py_DECREF(dict);
        return NULL;
    }

    // Now set nodes internal block
    patricia_node_t *node = NULL;
    size_t count = 0;
    PATRICIA_WALK_ALL (self->m_tree->head, node) {
        count += 1;
    } PATRICIA_WALK_END;

    bytes = PyBytes_FromStringAndSize((const char*)self->m_tree->head, sizeof(patricia_node_t)*count);
    ret_err = PyDict_SetItemString(dict, "nodes", bytes);
    Py_DECREF(bytes);  // dictionary now owns the reference
    if(ret_err) {
        PyErr_SetString(PyExc_MemoryError, "error writing nodes to dictionary");
        Py_DECREF(dict);
        return NULL;
    }

    PyObject* list = PyList_New(count);
    if (!list) {
        PyErr_SetString(PyExc_MemoryError, "error allocating data list");
        return NULL;
    }

    count = 0;
    if (self->m_tree->head) {
        PATRICIA_WALK_ALL (self->m_tree->head, node) {
            // some nodes may be glued in middle and will have prefix, but not data
            // In case of no data, we'll leave the list entry as default (None)
            if(node->data) {
                // SET_ITEM steal reference so increment in advance to keep original
                Py_INCREF(node->data);
                PyList_SET_ITEM(list, count, node->data);
            }
            else {
                Py_INCREF(Py_None);
                PyList_SET_ITEM(list, count, Py_None);
            }
            count += 1;
        } PATRICIA_WALK_END;
    }

    ret_err = PyDict_SetItemString(dict, "data", list);
    Py_DECREF(list);  // dictionary now owns the reference
    if(ret_err) {
        PyErr_SetString(PyExc_MemoryError, "error writing data list to dictionary");
        Py_DECREF(dict);
        return NULL;
    }

    PyObject* out_tuple = PyTuple_Pack(3, (PyObject*)Py_TYPE(self), args, dict);
    Py_DECREF(args);
    Py_DECREF(dict);
    return out_tuple;
}

static PyObject* pytricia_setstate(PyTricia *self, PyObject *args) {
    PyObject *state;
    if (!PyArg_ParseTuple(args, "O", &state)) {
        return NULL;
    }
    if (!PyDict_Check(state)) {
      PyErr_SetString(PyExc_TypeError, "__setstate__ argument must be a dictionary");
      return NULL;
    }

    PyObject* bytes = PyDict_GetItemString(state, "tree");
    if (!bytes || !PyBytes_Check(bytes) || PyBytes_Size(bytes) != sizeof(patricia_tree_t)) {
      PyErr_SetString(PyExc_TypeError, "__setstate__ failed tree type checking");
      return NULL;
    }
    memcpy(self->m_tree, PyBytes_AsString(bytes), sizeof(patricia_tree_t));
    
    // restore head/node data
    PyObject* nodebytes = PyDict_GetItemString(state, "nodes");
    if (!PyBytes_Check(nodebytes)) {
        PyErr_SetString(PyExc_TypeError, "__setstate__ failed nodes type checking");
        return NULL;
    }
    if (PyBytes_Size(nodebytes)) {
        patricia_node_t* original_head = self->m_tree->head;
        self->m_tree->head = calloc(1, PyBytes_Size(nodebytes));
        ssize_t offset_bytes = (char*)self->m_tree->head - (char*)original_head;
        size_t num_nodes = PyBytes_Size(nodebytes) / sizeof(patricia_node_t);
        if(self->m_tree->head == NULL) {
            PyErr_SetString(PyExc_MemoryError, "__setstate__ error allocating space for nodes");
            return NULL;
        }
        memcpy(self->m_tree->head, PyBytes_AsString(nodebytes), PyBytes_Size(nodebytes));
 
        // Now re-write the links relative to the start of the contiguous memory block
        patricia_node_t *node = self->m_tree->head;
        for(size_t i=0; i<num_nodes; i++) {
            if(node->parent) {
                node->parent = (patricia_node_t*)((char*)node->parent + offset_bytes);
            }
            if(node->l) {
                node->l = (patricia_node_t*)((char*)node->l + offset_bytes);
            }
            if(node->r) {
                node->r = (patricia_node_t*)((char*)node->r + offset_bytes);
            }
            node++;
        }
    }

    // Restore node data items
    PyObject* list = PyDict_GetItemString(state, "data");
    if (!PyList_Check(list)) {
        PyErr_SetString(PyExc_TypeError, "__setstate__ data is not list as expected!");
        return NULL;
    }
    else {
        Py_ssize_t list_len = PyList_Size(list);
        if (list_len * (Py_ssize_t)sizeof(patricia_node_t) != PyBytes_Size(nodebytes)) {
            PyErr_SetString(PyExc_TypeError, "__setstate__ node and data list sizes inconsistent!");
            return NULL;
        }
    }
    patricia_node_t *node = NULL;
    size_t count = 0;
    if (self->m_tree->head) {
        PATRICIA_WALK_ALL (self->m_tree->head, node) {
            node->data = PyList_GET_ITEM(list, count);
            Py_INCREF(node->data); // make our own reference
            // in special case of glue node with data value of None
            // we'll want to remove python None and instead use NULL
            if (node->data == Py_None) {
                Py_DECREF(node->data);
                node->data = NULL;
            }
            count += 1;
        } PATRICIA_WALK_END;
    }

    Py_RETURN_NONE;
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
    {"freeze", (PyCFunction)pytricia_freeze, METH_NOARGS, "freeze() -> \nCompacts pytricia object for efficient access, but disallows updates"},
    {"thaw", (PyCFunction)pytricia_thaw, METH_NOARGS, "thaw() -> \nreverses a frozen pytricia object to allow updates"},
    {"__reduce__", (PyCFunction)pytricia_reduce, METH_NOARGS, "Return state information for pickling"},
    {"__setstate__", (PyCFunction)pytricia_setstate, METH_VARARGS, "Set state information for unpickling"},
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

            if (iter->m_Xnode->data) {
                return _prefix_to_key_object(&iter->m_Xnode->prefix, iter->m_parent->m_raw_output);
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


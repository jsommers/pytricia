#include <arpa/inet.h>
#include <Python.h>
#include <string.h>
#include "patricia.h"

/*
 * - documentation
 * - any optimization
 * - check with python3 ...
 * - allow more flexible calling of methods; ints, ipaddrs, etc.
 * - error checking
 */

typedef struct {
    PyObject_HEAD
    patricia_tree_t *m_tree;
} PyTricia;

typedef struct {
    PyObject_HEAD
    patricia_tree_t *m_tree;
    patricia_node_t *m_Xnode;
    patricia_node_t *m_Xhead;
    patricia_node_t **m_Xstack;
    patricia_node_t **m_Xsp;
    patricia_node_t *m_Xrn;
} PyTriciaIter;

// Takes a PyObject and returns the IP Address contained in IPV.4 notation.
static char* get_str(PyObject *key) {
    // Need to deal with refcounts & handle possible errors for conversions.

    /*
    #if PY_MAJOR_VERSION >= 3
        // Python 3 case handling.
        // Pull unicode object out.
        return 
    #else
        // Python 2
    #endif
    */

    if (PyString_Check(key)) {
        char* keystr = malloc(sizeof(char) * PyString_Size(key));
        keystr = PyString_AsString(key);
        return keystr; // Converts PyObject to char*
        //return PyString_AsString(key);
    }
    else if (PyInt_Check(key)) {
        long val = htonl(PyInt_AsLong(key));
        char* keystr = malloc(sizeof(char) * 32);
        int p = sprintf(keystr, "%d.%d.%d.%d/32", val&0x000000ff, (val >> 8) & 0x000000ff, 
            (val >> 16) & 0x000000ff, (val >> 24) & 0x000000ff);
        if (p < 0) {
            printf("Error: Could not parse string. \n");
        }
        return keystr;
    }
}

static void
pytricia_dealloc(PyTricia* self)
{
    if (self) {
        Destroy_Patricia(self->m_tree, 0);
        Py_TYPE(self)->tp_free((PyObject*)self);
    }
}

static PyObject *
pytricia_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyTricia *self;

    self = (PyTricia*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->m_tree = NULL;
    }
    return (PyObject *)self;
}

static int
pytricia_init(PyTricia *self, PyObject *args, PyObject *kwds)
{
    int prefixlen = 32;
    if (!PyArg_ParseTuple(args, "|i", &prefixlen)) {
        self->m_tree = New_Patricia(1); // need to have *something* to dealloc
        PyErr_SetString(PyExc_ValueError, "Error parsing prefix length.");
        return -1;
    }

    if (prefixlen < 0 || prefixlen > PATRICIA_MAXBITS) {
        self->m_tree = New_Patricia(1); // need to have *something* to dealloc
        PyErr_SetString(PyExc_ValueError, "Invalid number of maximum bits; must be between 0 and 128, inclusive.");
        return -1;
    } 
    
    self->m_tree = New_Patricia(prefixlen);
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

/* from SubnetTree.cc */
static prefix_t* 
make_prefix(unsigned long addr, int width)
{
    prefix_t* subnet = (prefix_t*)malloc(sizeof(prefix_t));
    if (subnet == NULL) {
        return NULL;
    }

    memcpy(&subnet->add.sin, &addr, sizeof(subnet->add.sin)) ;
    subnet->family = AF_INET;
    subnet->bitlen = width;
    subnet->ref_count = 1;

    return subnet;
}

static int 
parse_cidr(const char *cidr, unsigned long *subnet, unsigned short *masklen)
{
    static char buffer[32];
    struct in_addr addr;

    if (!cidr) {
        return -1;
    }

    const char *s = strchr(cidr, '/');
    if (s) {
        unsigned long len = s - cidr < 32 ? s - cidr : 31;
        memcpy(buffer, cidr, len);
        buffer[len] = '\0';
        *masklen = atoi(s+1);
        s = buffer;
    } else {
        s = cidr;
        *masklen = 32;
    }

    if (!inet_aton((char *)(s), &addr)) {
        return -1;
    }

    *subnet = addr.s_addr;
    return 0;
}

//------------------------------------------------------
//------------------------------------------------------
//------------------------------------------------------
//------------------------------------------------------
//------------------------------------------------------
static prefix_t*
pystr_to_prefix(/*PyObject *pystr*/char* str) // Changed to take in a string instead of a pystr.
{
    /*
    char *cstraddr = NULL;
    if (!PyArg_Parse(pystr, "s", &cstraddr)) {
        return NULL;
    }
    */
    
    if (!str) { // If str isn't populated for some reason.
        return NULL;
    }

    unsigned long subnet = 0UL;
    unsigned short mask = 0;
    if (parse_cidr(str, &subnet, &mask)) {
        return NULL;
    }
    
    return make_prefix(subnet, mask);
}
//------------------------------------------------------
//------------------------------------------------------
//------------------------------------------------------
//------------------------------------------------------
//------------------------------------------------------

static PyObject* 
pytricia_subscript(PyTricia *self, PyObject *key)
{
    char* keystr = get_str(key); // Get string from PyObject.

    prefix_t* subnet = pystr_to_prefix(keystr);
    if (subnet == NULL) {
        PyErr_SetString(PyExc_ValueError, "Error parsing prefix.");
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
pytricia_internal_delete(PyTricia *self, PyObject *key)
{
    char* keystr = get_str(key); // Get string from PyObject.

    prefix_t* prefix = pystr_to_prefix(keystr);
    if (prefix == NULL) {
        PyErr_SetString(PyExc_ValueError, "Error parsing prefix.");
        return -1;
    }

    patricia_node_t* node = patricia_search_exact(self->m_tree, prefix);
    Deref_Prefix(prefix);

    if (!node) {
        PyErr_SetString(PyExc_KeyError, "Prefix doesn't exist.  Can't delete it.");
        return -1;
    }

    // decrement ref count on data referred to by key, if it exists
    PyObject* data = (PyObject*)node->data;
    Py_XDECREF(data);

    patricia_remove(self->m_tree, node);
    return 0;
}

static int 
pytricia_assign_subscript(PyTricia *self, PyObject *key, PyObject *value)
{
    if (!value) {
        return pytricia_internal_delete(self, key);
    }
    
    char *keystr = get_str(key);

    /* Taken out to test possible integer functionality.
    if (!PyArg_Parse(key, "s", &keystr)) {
        PyErr_SetString(PyExc_ValueError, "Error parsing prefix 4.");
        return -1;
    }
    */
    
    patricia_node_t* node = make_and_lookup(self->m_tree, keystr);
    if (!node) {
        PyErr_SetString(PyExc_ValueError, "Error inserting into patricia tree");
        return -1;
    }

    Py_INCREF(value);
    node->data = value;

    return 0;
}

static PyObject*
pytricia_insert(PyTricia *self, PyObject *args) {
    // When this is called it will be p[prefix] = obj.
    // Deal with issue when type may be different - can you do "OO" to make it more generic?
    // Python 3 has IP address & IP network object - figure out how to make this work.
    // 1 - make parsetuple work with ints too.
    // 2 - translate int to string. Everything is 32 bits but that's implicit.

    // Next thing: look at test code & how IP addresses are used - we might have to translate everything
    // in more places (ex: lookup).

    // Figure out how Python 3 objects work.
    // HOW TO FIGURE OUT WHAT AN OBJECT IS?!?!


    PyObject *key = NULL;
    PyObject *value = NULL;
    if (!PyArg_ParseTuple(args, "OO", &key, &value)) { // Changed from "sO"
        return NULL;
    }

    char* keystr = get_str(key);

    patricia_node_t* node = make_and_lookup(self->m_tree, keystr);
    if (!node) {
        PyErr_SetString(PyExc_ValueError, "Error inserting into patricia tree");
        return NULL;
    }

    // two increments of refcounts: value is stored in patricia tree,
    // as well as passed back to caller.

    Py_INCREF(value);
    node->data = value;

    Py_INCREF(value); // increase because you return it - you're basically making a new one.
    return value;
}

static PyObject*
pytricia_delitem(PyTricia *self, PyObject *args)
{
    PyObject *key = NULL;
    if (!PyArg_ParseTuple(args, "O", &key)) {
        PyErr_SetString(PyExc_ValueError, "Unclear what happened!");
        return NULL;
    }
    
    int rv = pytricia_internal_delete(self, key);
    if (rv < 0) {
        return NULL;
    } 
    Py_RETURN_NONE;
}

static PyObject *
pytricia_get(register PyTricia *obj, PyObject *args)
{
    PyObject *key = NULL;
    PyObject *defvalue = NULL;

    if (!PyArg_ParseTuple(args, "O|O:get", &key, &defvalue))
        return NULL;

    char* keystr = get_str(key); // Get string from PyObject.

    prefix_t* prefix = pystr_to_prefix(keystr);
    if (prefix == NULL) {
        PyErr_SetString(PyExc_ValueError, "Error parsing prefix.");
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

static int
pytricia_contains(PyTricia *self, PyObject *key)
{
    char* keystr = get_str(key); // Get string from PyObject.
    prefix_t* prefix = pystr_to_prefix(keystr);
    if (!prefix) {
        PyErr_SetString(PyExc_ValueError, "Error parsing prefix.");
        return -1;
    }
    
    patricia_node_t* node = patricia_search_best(self->m_tree, prefix);
    Deref_Prefix(prefix);
    if (node) {
        return 1;
    }
    return 0;
}

static PyObject*
pytricia_has_key(PyTricia *self, PyObject *args)
{
    PyObject *key = NULL;
    if (!PyArg_ParseTuple(args, "O", &key))
        return NULL;
        
    char* keystr = get_str(key); // Get string from PyObject.

    prefix_t* prefix = pystr_to_prefix(keystr);
    if (!prefix) {
        PyErr_SetString(PyExc_ValueError, "Error parsing prefix.");
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
pytricia_keys(register PyTricia *self, PyObject *unused)
{
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
    {"has_key",   (PyCFunction)pytricia_has_key, METH_VARARGS, "has_key(prefix) -> boolean\nReturn true iff prefix is in tree.  Note that this method checks for an *exact* match with the prefix.\nUse the 'in' operator if you want to test whether a given address is 'valid' in the tree."},
    {"keys",   (PyCFunction)pytricia_keys, METH_NOARGS, "keys() -> list\nReturn a list of all prefixes in the tree."},
    {"get", (PyCFunction)pytricia_get, METH_VARARGS, "get(prefix, [default]) -> object\nReturn value associated with prefix."},
    {"delete", (PyCFunction)pytricia_delitem, METH_VARARGS, "delete(prefix) -> \nDelete mapping associated with prefix.\n"},
    {"insert", (PyCFunction)pytricia_insert, METH_VARARGS, "insert(prefix, data) -> data\nCreate mapping between prefix and data in tree."},
    // Prefix can be single IP address. Prefix length is assumed to be 32.
    // When someone gives us something to add, is it a string? Detects whether prefix or not - if not, it's 32.
    // We should accept integers.
    // "10.1.3.5/32" -> "10.1.3.5" -> 0x0a010305 -> 0000 1010 0000 0001 0000 0011 0000 0101
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
    if (iterobj->m_tree) {
        Py_DECREF(iterobj->m_tree);
    }
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

    iterobj->m_tree = self->m_tree;
    Py_INCREF(self);
    iterobj->m_Xnode = NULL;
    iterobj->m_Xhead = iterobj->m_tree->head;
    iterobj->m_Xstack = (patricia_node_t**) malloc(sizeof(patricia_node_t*)*(PATRICIA_MAXBITS+1));
    if (!iterobj->m_Xstack) {
        Py_DECREF(self);
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
#if PY_MAJOR_VERSION >= 3
        return NULL;
#else
        return;
#endif

    PyTriciaIterType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyTriciaIterType) < 0)
#if PY_MAJOR_VERSION >= 3
        return NULL;
#else
        return;
#endif

#if PY_MAJOR_VERSION >= 3
    m = PyModule_Create(&pytricia_moduledef);
#else
    m = Py_InitModule3("pytricia", NULL, pytricia_doc);
#endif
    if (m == NULL)
#if PY_MAJOR_VERSION >= 3 // Conditional compile statements - put python 2 specific in else block.
      return NULL;
#else
      return;
#endif

    Py_INCREF(&PyTriciaType);
    Py_INCREF(&PyTriciaIterType);
    PyModule_AddObject(m, "PyTricia", (PyObject *)&PyTriciaType);

    // JS: don't add this object to the public interface.  users shouldn't be
    // able to create iterator objects w/o calling __iter__ on a pytricia object.
    // PyModule_AddObject(m, "PyTriciaIter", (PyObject *)&PyTriciaIterType);

#if PY_MAJOR_VERSION >= 3
    return m;
#endif
}


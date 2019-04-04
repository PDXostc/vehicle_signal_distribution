%module vsd_swig
%{

#include <vehicle_signal_distribution.h>
#include "../vsd_internal.h"
#include <stdint.h>
#include <errno.h>

    static uint8_t do_callback(vsd_signal_node_t* node, void* v_ctx)
    {
        vsd_context_t* ctx = (vsd_context_t*) v_ctx;
        PyObject *arglist = 0;
        PyObject *result = 0;
        PyObject *cb = (PyObject*) vsd_context_get_user_data(ctx);
        vsd_signal_t* elem = node->data;
        char* sig_name = vsd_signal_to_path_static(elem);

        if (vsd_elem_type(elem) == vsd_branch) {
            printf("FATAL: Tried to print branch: %u:%s", vsd_id(elem), vsd_name(elem));
            exit(255);
        }


        switch(vsd_data_type(elem)) {
        case vsd_int8:
            arglist = Py_BuildValue("Isb", elem->id, sig_name, vsd_value(elem).i8);
            break;
        case vsd_int16:
            arglist = Py_BuildValue("Ish", elem->id, sig_name, vsd_value(elem).i16);
            break;
        case vsd_int32:
            arglist = Py_BuildValue("Isi", elem->id, sig_name, vsd_value(elem).i32);
            break;
        case vsd_uint8:
            arglist = Py_BuildValue("IsB", elem->id, sig_name, vsd_value(elem).i8);
            break;
        case vsd_uint16:
            arglist = Py_BuildValue("IsH", elem->id, sig_name, vsd_value(elem).i16);
            break;
        case vsd_uint32:
            arglist = Py_BuildValue("IsI", elem->id, sig_name, vsd_value(elem).i32);
            break;
        case vsd_double:
            arglist = Py_BuildValue("Isd", elem->id, sig_name, vsd_value(elem).d);
            break;
        case vsd_float:
            arglist = Py_BuildValue("Isf", elem->id, sig_name, vsd_value(elem).f);
            break;
        case vsd_boolean:
            arglist = Py_BuildValue("Isb", elem->id, sig_name, vsd_value(elem).b);
            break;
        case vsd_string:
            arglist = Py_BuildValue("Isy#", elem->id, sig_name,
                                    vsd_value(elem).s.data,
                                    vsd_value(elem).s.len);
            break;
        case vsd_na:
            break;
        default:
            break;
        }

        if (arglist) {
            result = PyObject_CallObject(cb, arglist);
            Py_DECREF(arglist);
            if (result)
                Py_DECREF(result);
        }
        return 1;
    }

    void swig_vsd_process(vsd_context_t* ctx, vsd_signal_list_t* lst)
    {
        vsd_signal_list_for_each(lst, do_callback, ctx);
        return;
    }
%}

%include "typemaps.i"

%inline %{
    typedef unsigned int vsd_id_t;
    typedef long int usec_timestamp_t;

    extern int dstc_process_events(usec_timestamp_t);

    extern vsd_data_type_e vsd_data_type(vsd_signal_t* sig);
    extern int vsd_publish(vsd_signal_t* sig);
    extern int vsd_set_value_by_path_int8(vsd_context_t* context,
                                          char* path,
                                          signed char val);
    extern vsd_data_u vsd_value(vsd_signal_t* sig);
    extern int vsd_load_from_file(vsd_context_t* ctx, char *fname);
    extern int vsd_subscribe(struct vsd_context* ctx,
                             struct vsd_signal* sig,
                             vsd_subscriber_cb_t callback);

    extern char* vsd_signal_to_path_static(struct vsd_signal* desc);

    vsd_context_t* swig_vsd_create_context(void)
    {
        struct vsd_context* ctx = 0;
        vsd_context_create(&ctx);
        return ctx;
    }

    vsd_signal_t* swig_vsd_find_signal_by_path(vsd_context_t* ctx, char* path)
    {
        vsd_signal_t* res = 0;
        if (vsd_find_desc_by_path(ctx, 0, path, &res))
            return 0;

        return res;
    }

    vsd_signal_t* swig_vsd_find_signal_by_id(vsd_context_t* ctx, vsd_id_t id)
    {
        vsd_signal_t* res = 0;
        if (vsd_find_desc_by_id(ctx, id, &res))
            return 0;

        return res;
    }

    int  swig_vsd_subscribe(vsd_context_t* ctx, vsd_signal_t* sig)
    {
        return vsd_subscribe(ctx, sig, swig_vsd_process);
    }

    int swig_vsd_data_type(vsd_signal_t* sig)
    {
        return (int) vsd_data_type(sig);
    }

    void swig_vsd_set_python_callback(vsd_context_t* ctx , PyObject* cb)
    {
        PyObject *o_cb = (PyObject*) vsd_context_get_user_data(ctx);

        // Wipe old value, if set.
        if (o_cb)
            Py_DECREF(o_cb);

        Py_INCREF(cb);
        vsd_context_set_user_data(ctx, (void*) cb);
    }

    signed char swig_vsd_value_i8(vsd_context_t* ctx, vsd_signal_t* sig) {
        return vsd_value(sig).i8;
    }

    unsigned char swig_vsd_value_u8(vsd_context_t* ctx, vsd_signal_t* sig) {
        return vsd_value(sig).u8;
    }

    signed short swig_vsd_value_i16(vsd_context_t* ctx, vsd_signal_t* sig) {
        return vsd_value(sig).i16;
    }

    unsigned short swig_vsd_value_u16(vsd_context_t* ctx, vsd_signal_t* sig) {
        return vsd_value(sig).u16;
    }

    signed int swig_vsd_value_i32(vsd_context_t* ctx, vsd_signal_t* sig) {
        return vsd_value(sig).i32;
    }

    unsigned int swig_vsd_value_u32(vsd_context_t* ctx, vsd_signal_t* sig) {
        return vsd_value(sig).u32;
    }

    float swig_vsd_value_f(vsd_context_t* ctx, vsd_signal_t* sig) {
        return vsd_value(sig).f;
    }

    double swig_vsd_value_d(vsd_context_t* ctx, vsd_signal_t* sig) {
        return vsd_value(sig).d;
    }

    unsigned int swig_vsd_value_b(vsd_context_t* ctx, vsd_signal_t* sig) {
        return vsd_value(sig).b;
    }

    //
    // SET VALUE
    //
    signed char swig_vsd_set_i8(vsd_context_t* ctx, vsd_signal_t* sig, signed char val)
    {
        return vsd_set_value_by_desc_int8(ctx, sig, val);
    }

    unsigned char swig_vsd_set_u8(vsd_context_t* ctx, vsd_signal_t* sig, unsigned char val)
    {
        return vsd_set_value_by_desc_uint8(ctx, sig, val);
    }

    signed short swig_vsd_set_i16(vsd_context_t* ctx, vsd_signal_t* sig, signed short val)
    {
        return vsd_set_value_by_desc_int16(ctx, sig, val);
    }

    unsigned short swig_vsd_set_u16(vsd_context_t* ctx, vsd_signal_t* sig, unsigned short val)
    {
        return vsd_set_value_by_desc_uint16(ctx, sig, val);
    }

    signed int swig_vsd_set_i32(vsd_context_t* ctx, vsd_signal_t* sig, signed int val)
    {
        return vsd_set_value_by_desc_int32(ctx, sig, val);
    }

    unsigned int swig_vsd_set_u32(vsd_context_t* ctx, vsd_signal_t* sig, unsigned int val)
    {
        return vsd_set_value_by_desc_uint32(ctx, sig, val);
    }

    float swig_vsd_set_f(vsd_context_t* ctx, vsd_signal_t* sig, float val)
    {
        return vsd_set_value_by_desc_float(ctx, sig, val);
    }

    double swig_vsd_set_d(vsd_context_t* ctx, vsd_signal_t* sig, double val)
    {
        return vsd_set_value_by_desc_double(ctx, sig, val);
    }

    unsigned int swig_vsd_set_b(vsd_context_t* ctx, vsd_signal_t* sig, signed char val)
    {
        return vsd_set_value_by_desc_boolean(ctx, sig, val);
    }


    %}

%include "cstring.i"
%cstring_output_allocate_size(char** str, int *len, free(*$1));
%{
    int swig_vsd_value_s(vsd_context_t* ctx, vsd_signal_t* sig, char** str, int *len) {
        *str = (char*) malloc(vsd_value(sig).s.len);
        *len = vsd_value(sig).s.len;
        memcpy(*str, vsd_value(sig).s.data, *len);
        return 0;
}
%}

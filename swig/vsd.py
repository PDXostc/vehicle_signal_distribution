#
# Copyright (C) 2018, Jaguar Land Rover
# This program is licensed under the terms and conditions of the
# Mozilla Public License, version 2.0.  The full text of the
# Mozilla Public License is at https:#www.mozilla.org/MPL/2.0/
#
# Author: Magnus Feuer (mfeuer1@jaguarlandrover.com)

#
# Simple library to integrate Python with DSTC
#
import dstc
import vsd_swig

_callback = None
# Copied in from vehicle_signal_distribution.h
# Could not get exported enums to work.
class data_type_e:
    vsd_int8 = 0
    vsd_uint8 = 1
    vsd_int16 = 2
    vsd_uint16 = 3
    vsd_int32 = 4
    vsd_uint32 = 5
    vsd_double = 6
    vsd_float = 7
    vsd_boolean = 8
    vsd_string = 9
    vsd_stream = 10
    vsd_na = 11

def create_context():
    return vsd_swig.swig_vsd_create_context()

def publish(sig):
    return vsd_swig.vsd_publish(sig)

def subscribe(ctx, sig):
    return vsd_swig.swig_vsd_subscribe(ctx, sig)

def load_from_file(ctx, fname):
    return vsd_swig.vsd_load_from_file(ctx, fname)

def signal_by_id(ctx, sig_id):
    return vsd_swig.swig_vsd_find_signal_by_path(ctx, sig_id)

def signal(ctx, path):
    return vsd_swig.swig_vsd_find_signal_by_path(ctx, path)

def path(sig):
    return vsd_swigt.vsd_desc_to_path_static(sig)

def get(ctx, sig):
    dt = vsd_swig.swig_vsd_data_type(sig)

    if dt == data_type_e.vsd_int8:
        return vsd_swig.swig_vsd_value_i8(ctx, sig)

    if dt == data_type_e.vsd_uint8:
        return vsd_swig.swig_vsd_value_u8(ctx, sig)

    if dt == data_type_e.vsd_int16:
        return vsd_swig.swig_vsd_value_i16(ctx, sig)

    if dt == data_type_e.vsd_uint16:
        return vsd_swig.swig_vsd_value_u16(ctx, sig)

    if dt == data_type_e.vsd_int32:
        return vsd_swig.swig_vsd_value_i32(ctx, sig)

    if dt == data_type_e.vsd_uint32:
        return vsd_swig.swig_vsd_value_u32(ctx, sig)

    if dt == data_type_e.vsd_double:
        return vsd_swig.swig_vsd_value_d(ctx, sig)

    if dt == data_type_e.vsd_float:
        return vsd_swig.swig_vsd_value_f(ctx, sig)

    if dt == data_type_e.vsd_boolean:
        return vsd_swig.swig_vsd_value_b(ctx, sig)

    if dt == data_type_e.vsd_string:
        return vsd_swig.swig_vsd_value_s(ctx, sig)

    if dt == data_type_e.vsd_stream:
        return None

    if dt == data_type_e.vsd_na:
        return None

    return None


def set(ctx, sig, val):
    dt = vsd_swig.swig_vsd_data_type(sig)

    if dt == data_type_e.vsd_int8:
        return vsd_swig.swig_vsd_set_i8(ctx, sig, val)

    if dt == data_type_e.vsd_uint8:
        return vsd_swig.swig_vsd_set_u8(ctx, sig, val)

    if dt == data_type_e.vsd_int16:
        return vsd_swig.swig_vsd_set_i16(ctx, sig, val)

    if dt == data_type_e.vsd_uint16:
        return vsd_swig.swig_vsd_set_u16(ctx, sig, val)

    if dt == data_type_e.vsd_int32:
        return vsd_swig.swig_vsd_set_i32(ctx, sig, val)

    if dt == data_type_e.vsd_uint32:
        return vsd_swig.swig_vsd_set_u32(ctx, sig, val)

    if dt == data_type_e.vsd_double:
        return vsd_swig.swig_vsd_set_d(ctx, sig, val)

    if dt == data_type_e.vsd_float:
        return vsd_swig.swig_vsd_set_f(ctx, sig, val)

    if dt == data_type_e.vsd_boolean:
        return vsd_swig.swig_vsd_set_b(ctx, sig, val)

    if dt == data_type_e.vsd_string:
        return vsd_swig.swig_vsd_set_s(ctx, sig, val)

    if dt == data_type_e.vsd_stream:
        return None

    if dt == data_type_e.vsd_na:
        return None

    return None

def _intermediate_callback(*arg):
    # Invoke the real callback with unpacked tuples provided
    # as native arguments.

    (signal_id, path, value) = arg
    _callback(signal_id, path, value)

def set_callback(ctx, cb):
    global _callback
    _callback = cb
    return vsd_swig.swig_vsd_set_python_callback(ctx , _intermediate_callback)

def process_events(timeout_msec):
    return vsd_swig.dstc_process_events(timeout_msec);

dstc.activate()

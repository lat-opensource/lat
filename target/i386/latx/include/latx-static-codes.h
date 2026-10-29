/*
 * Compatibility surface for Quicktrans on this single-context LATX host.
 */
#ifndef LATX_STATIC_CODES_H
#define LATX_STATIC_CODES_H

#include "translate.h"

/* This host has one native-to-BT return-0 entry, not a per-context table. */
#define GET_SC_TABLE(_mtcc_id, _entry) (context_switch_native_to_bt_ret_0)

#endif

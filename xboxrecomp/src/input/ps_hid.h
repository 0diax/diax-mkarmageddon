/* PlayStation controllers (DualShock 4, DualSense) read straight from HID (ps_hid.c).
 *
 * Windows gives them no XInput slot, so without Steam or a driver of its own a DualSense is no controller at all to
 * a title. Here each one found is read on a thread of its own, over USB or Bluetooth, and reported in the Xbox
 * pad's terms. */

#ifndef PS_HID_H
#define PS_HID_H

#include "xinput_xbox.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The `index`th PlayStation controller connected (in the order they were found), as an Xbox pad; 0 when it is
 * there, ERROR_DEVICE_NOT_CONNECTED when not. */
DWORD ps_hid_get_state(unsigned index, XBOX_INPUT_STATE *state);
/* That controller's motors: left the strong (low-frequency) one, right the weak, 0..65535 as XInput's. */
void ps_hid_set_rumble(unsigned index, WORD left, WORD right);

#ifdef __cplusplus
}
#endif

#endif

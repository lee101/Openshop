#ifndef APP_HOTKEY_H
#define APP_HOTKEY_H

int app_hotkey_matches(int key, int ctrl, int alt, int shift,
                       int expected_key, int expected_ctrl, int expected_alt, int expected_shift);

#endif

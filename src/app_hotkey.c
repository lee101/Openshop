#include "app_hotkey.h"

int app_hotkey_matches(int key, int ctrl, int alt, int shift,
                       int expected_key, int expected_ctrl, int expected_alt, int expected_shift) {
    return key == expected_key &&
           ctrl == expected_ctrl &&
           alt == expected_alt &&
           shift == expected_shift;
}

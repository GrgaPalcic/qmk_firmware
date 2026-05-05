/*
Copyright 2023 @ Nuphy <https://nuphy.com/>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "user_kb.h"
#include "ansi.h"
#include "mcu_pwr.h"
#include "version.h"

char            socd_type[4][14]  = { "disabled", "cancellation", "exclusion", "nullification" };

typedef enum {
    SOCD_SIDE_NONE = 0,
    SOCD_SIDE_LEFT,
    SOCD_SIDE_RIGHT,
} socd_side_t;

typedef struct {
    uint16_t left;
    uint16_t right;
} socd_pair_t;

typedef struct {
    bool        physical_left;
    bool        physical_right;
    bool        logical_left;
    bool        logical_right;
    socd_side_t last_pressed;
    socd_side_t blocked_side;
} socd_state_t;

static const socd_pair_t socd_pairs[] = {
    { KC_A, KC_D },
    { KC_LEFT, KC_RIGHT },
    { KC_W, KC_S },
    { KC_UP, KC_DOWN },
};

static socd_state_t socd_states[sizeof_array(socd_pairs)] = {0};

static socd_side_t socd_opposite_side(socd_side_t side) {
    if (side == SOCD_SIDE_LEFT) { return SOCD_SIDE_RIGHT; }
    if (side == SOCD_SIDE_RIGHT) { return SOCD_SIDE_LEFT; }
    return SOCD_SIDE_NONE;
}

static bool socd_find_pair(uint16_t keycode, uint8_t *pair_idx, socd_side_t *side) {
    for (uint8_t idx = 0; idx < sizeof_array(socd_pairs); ++idx) {
        if (keycode == socd_pairs[idx].left) {
            *pair_idx = idx;
            *side     = SOCD_SIDE_LEFT;
            return true;
        }

        if (keycode == socd_pairs[idx].right) {
            *pair_idx = idx;
            *side     = SOCD_SIDE_RIGHT;
            return true;
        }
    }

    return false;
}

static socd_side_t socd_current_winner(const socd_state_t *state) {
    if (state->last_pressed == SOCD_SIDE_LEFT || state->last_pressed == SOCD_SIDE_RIGHT) {
        return state->last_pressed;
    }

    if (state->logical_left) { return SOCD_SIDE_LEFT; }
    if (state->logical_right) { return SOCD_SIDE_RIGHT; }

    return SOCD_SIDE_LEFT;
}

static void socd_apply_output(uint8_t pair_idx, bool want_left, bool want_right) {
    socd_state_t       *state = &socd_states[pair_idx];
    const socd_pair_t  *pair  = &socd_pairs[pair_idx];

    if (state->logical_left && !want_left) {
        unregister_code16(pair->left);
    }

    if (state->logical_right && !want_right) {
        unregister_code16(pair->right);
    }

    if (!state->logical_left && want_left) {
        register_code16(pair->left);
    }

    if (!state->logical_right && want_right) {
        register_code16(pair->right);
    }

    state->logical_left  = want_left;
    state->logical_right = want_right;
}

static void socd_update_pair(uint8_t pair_idx) {
    socd_state_t *state      = &socd_states[pair_idx];
    bool          want_left  = false;
    bool          want_right = false;

    if (!state->physical_left && !state->physical_right) {
        state->last_pressed = SOCD_SIDE_NONE;
        state->blocked_side = SOCD_SIDE_NONE;
        socd_apply_output(pair_idx, false, false);
        return;
    }

    switch (user_config.socd_mode) {
        case 1: {
            if (state->blocked_side == SOCD_SIDE_LEFT && !state->physical_left) {
                state->blocked_side = SOCD_SIDE_NONE;
            } else if (state->blocked_side == SOCD_SIDE_RIGHT && !state->physical_right) {
                state->blocked_side = SOCD_SIDE_NONE;
            }

            if (state->physical_left && state->physical_right) {
                socd_side_t winner = socd_current_winner(state);
                state->blocked_side = socd_opposite_side(winner);
                want_left  = winner == SOCD_SIDE_LEFT;
                want_right = winner == SOCD_SIDE_RIGHT;
            } else if (state->physical_left) {
                want_left = state->blocked_side != SOCD_SIDE_LEFT;
            } else {
                want_right = state->blocked_side != SOCD_SIDE_RIGHT;
            }
            break;
        }

        case 2: {
            state->blocked_side = SOCD_SIDE_NONE;
            if (state->physical_left && state->physical_right) {
                socd_side_t winner = socd_current_winner(state);
                want_left  = winner == SOCD_SIDE_LEFT;
                want_right = winner == SOCD_SIDE_RIGHT;
            } else {
                want_left  = state->physical_left;
                want_right = state->physical_right;
            }
            break;
        }

        case 3: {
            state->blocked_side = SOCD_SIDE_NONE;
            if (state->physical_left != state->physical_right) {
                want_left  = state->physical_left;
                want_right = state->physical_right;
            }
            break;
        }

        default: {
            state->blocked_side = SOCD_SIDE_NONE;
            want_left  = state->physical_left;
            want_right = state->physical_right;
            break;
        }
    }

    socd_apply_output(pair_idx, want_left, want_right);
}

void socd_reset_state(void) {
    memset(socd_states, 0, sizeof(socd_states));
}

/* qmk pre-process record */
bool pre_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    no_act_time      = 0;
    rf_linking_time  = 0;


    // wake up immediately
    if (f_wakeup_prepare) {
        exit_light_sleep(false);
        // f_wakeup_prepare = 0;
    }

    if (!pre_process_record_user(keycode, record)) {
        return false;
    }

    return true;
}

bool process_record_socd(uint16_t keycode, keyrecord_t *record) {
    if (user_config.socd_mode == 0) { return true; }

    uint8_t     pair_idx = 0;
    socd_side_t side     = SOCD_SIDE_NONE;

    if (!socd_find_pair(keycode, &pair_idx, &side)) {
        return true;
    }

    socd_state_t *state = &socd_states[pair_idx];

    if (side == SOCD_SIDE_LEFT) {
        state->physical_left = record->event.pressed;
    } else {
        state->physical_right = record->event.pressed;
    }

    if (record->event.pressed) {
        state->last_pressed = side;
    }

    socd_update_pair(pair_idx);

    if (!state->physical_left && !state->physical_right) {
        state->last_pressed = SOCD_SIDE_NONE;
        state->blocked_side = SOCD_SIDE_NONE;
    }

    return false;
}

/* qmk process record user*/
bool process_record_user(uint16_t keycode, keyrecord_t *record) {

    switch (keycode) {
        case SIDE_VAI:
        case SIDE_VAD:
        case SIDE_HUI:
        case DEBOUNCE_I:
        case DEBOUNCE_D:
        case DEBOUNCE_T:
        case SOCD_TOG:
        case RF_DFU:
            call_update_eeprom_data(&user_update);
            return true;

        case NUMLOCK_IND:
            if (game_mode_enable) { return true; }
            call_update_eeprom_data(&user_update);
            return true;

        case SIDE_MOD:
        case SIDE_SPI:
        case SIDE_SPD:
        case SIDE_1:
        case SLEEP_MODE:
        case SLEEP_I:
        case SLEEP_D:
        case CAPS_WORD:
            if (game_mode_enable) { return false; }
            call_update_eeprom_data(&user_update);
            return true;

        case BAT_SHOW:
        case SLEEP_NOW:
            if (game_mode_enable) { return false; }
            return true;

        case RGB_TOG:
            if (game_mode_enable) { return true; }
            call_update_eeprom_data(&rgb_update);
            return true;

        case RGB_VAI:
        case RGB_VAD:
        case RGB_SAI:
        case RGB_SAD:
        case RGB_HUI:
        case RGB_HUD:
        case RGB_MOD:
        case RGB_RMOD:
            if (game_mode_enable) {
                call_update_eeprom_data(&user_update);
                return true;
            }
            call_update_eeprom_data(&rgb_update);
            return true;

        case RGB_SPI:
        case RGB_SPD:
        case RGB_M_P:
            if (game_mode_enable) { return false; }
            call_update_eeprom_data(&rgb_update);
            return true;

        default:
            return true;
    }
}


/* qmk process record */
bool process_record_kb(uint16_t keycode, keyrecord_t *record) {

    if (!process_record_user(keycode, record)) {
        return false;
    }

    if (!process_record_socd(keycode, record)) {
        return false;
    }

    switch (keycode) {
        case RF_DFU:
            if (game_mode_enable) { return false; }
            if (record->event.pressed) {
                f_rf_dfu_press = 1;
            } else if (f_rf_dfu_press) {
                f_rf_dfu_press = 0;
                user_config.rf_delay_step = (user_config.rf_delay_step + 1) % 5;
#ifndef NO_DEBUG
                dprintf("rf_delay: %d\n", RF_POWER_DOWN_DELAY);
#endif
                signal_rgb_led(user_config.rf_delay_step * 2, 0, led_idx.RF_DFU, UINT8_MAX, 3000);
            }
            return false;

        case LNK_USB:
            if (record->event.pressed) {
                break_all_key();
            } else {
                dev_info.link_mode = LINK_USB;
                uart_send_cmd(CMD_SET_LINK, 10, 10);
            }
            return false;

        case LNK_RF ... LNK_BLE3:
            if (record->event.pressed) {
                if (dev_info.link_mode != LINK_USB) {
                    rf_sw_temp    = keycode - LNK_RF;
                    f_rf_sw_press = 1;
                    break_all_key();
                }
            } else {
                if (f_rf_sw_press) {
                    set_link_mode();
                    uart_send_cmd(CMD_SET_LINK, 10, 20);
                }
                for (uint8_t i = 1; i <= 4; i++) {
                    rgb_matrix_set_color(i, RGB_OFF);
                }
                rgb_matrix_update_pwm_buffers();
            }
            return false;

        case GAME_MODE:
            if (record->event.pressed) {
                f_gmode_reset_press = 1;
            } else {
                if (f_gmode_reset_press) {
                    f_gmode_reset_press = 0;
                    game_mode_enable = !game_mode_enable;
                    game_mode_tweak();
                }
            }
            return false;

        case CAPS_WORD:
            f_caps_word_tg = record->event.pressed;
            return false;

        case KC_LGUI:
        case WIN_LOCK:
            if (record->event.pressed) {
                if (get_highest_layer(layer_state) == M_LAYER || keycode == WIN_LOCK) {
                    keymap_config.no_gui = !keymap_config.no_gui;
                    signal_rgb_led(!keymap_config.no_gui, 1, led_idx.KC_LGUI, UINT8_MAX, 3000);
                    return false;
                }
            }
            return true;

        case KC_LSFT:
            if (!record->event.pressed) {
                if ((!user_config.caps_word_enable || game_mode_enable) && is_caps_word_on()) { caps_word_off(); }
            }
            return true;

        case MAC_VOICE:
            if (record->event.pressed) {
                if (dev_info.sys_sw_state == SYS_SW_MAC) {
                    host_consumer_send(0xcf);
                } else {
                    tap_code(KC_F5);
                }
            } else if (dev_info.sys_sw_state == SYS_SW_MAC) {
                host_consumer_send(0);
            }
            return false;

        case MAC_DND:
            if (record->event.pressed) {
                if (dev_info.sys_sw_state == SYS_SW_MAC) {
                    host_system_send(0x9b);
                }
            } else if (dev_info.sys_sw_state == SYS_SW_MAC) {
                host_system_send(0);
            }
            return false;

        case TASK:
            if (record->event.pressed) {
                if (dev_info.sys_sw_state == SYS_SW_MAC) {
                    tap_code(KC_MCTL);
                } else {
                    tap_code(KC_CALC);
                }
            }
            return false;

        case SEARCH:
            if (record->event.pressed) {
                if (dev_info.sys_sw_state == SYS_SW_MAC) {
                    register_code(KC_LGUI);
                    register_code(KC_SPACE);
                    wait_ms(TAP_CODE_DELAY);
                    unregister_code(KC_LGUI);
                    unregister_code(KC_SPACE);
                } else {
                    register_code(KC_LCTL);
                    register_code(KC_F);
                    wait_ms(TAP_CODE_DELAY);
                    unregister_code(KC_F);
                    unregister_code(KC_LCTL);
                }
            }
            return false;

        case PRT_SCR:
            if (record->event.pressed) {
                if (dev_info.sys_sw_state == SYS_SW_MAC) {
                    register_code(KC_LGUI);
                    register_code(KC_LSFT);
                    register_code(KC_3);
                    wait_ms(TAP_CODE_DELAY);
                    unregister_code(KC_3);
                    unregister_code(KC_LSFT);
                    unregister_code(KC_LGUI);
                } else {
                    tap_code(KC_PSCR);
                }
            }
            return false;

        case PRT_AREA:
            if (record->event.pressed) {
                if (dev_info.sys_sw_state == SYS_SW_MAC) {
                    register_code(KC_LGUI);
                    register_code(KC_LSFT);
                    register_code(KC_4);
                    wait_ms(TAP_CODE_DELAY);
                    unregister_code(KC_4);
                    unregister_code(KC_LSFT);
                    unregister_code(KC_LGUI);
                }
                else {
                    tap_code(KC_PSCR);
                }
            }
            return false;

        case SIDE_VAI:
        case SIDE_VAD:
            if (record->event.pressed) {
                uint8_t dir = keycode == SIDE_VAD ? 0 : 1;
                side_light_control(dir);
            }
            return false;

        case SIDE_MOD:
            if (record->event.pressed) {
                side_mode_control(1);
            }
            return false;

        case SIDE_HUI:
            if (record->event.pressed) {
                side_colour_control(1);
            }
            return false;

        case SIDE_SPI:
        case SIDE_SPD:
            if (record->event.pressed) {
                uint8_t dir = keycode == SIDE_SPD ? 0 : 1;
                side_speed_control(dir);
            }
            return false;

        case SIDE_1:
            if (record->event.pressed) {
                side_one_control(1);
            }
            return false;

        case RGB_VAI:
            if (record->event.pressed) {
                rgb_matrix_increase_val_noeeprom();
            }
            return false;

        case RGB_VAD:
            if (record->event.pressed) {
                rgb_matrix_decrease_val_noeeprom();
            }
            return false;

        case RGB_MOD:
            if (record->event.pressed) {
                if (game_mode_enable) {
                    rgb_matrix_step_game_mode(1);
                    return false;
                }
                rgb_matrix_step_noeeprom();
            }
            return false;

        case RGB_RMOD:
            if (record->event.pressed) {
                if (game_mode_enable) {
                    rgb_matrix_step_game_mode(0);
                    return false;
                }
                rgb_matrix_step_reverse_noeeprom();
            }
            return false;

        case RGB_HUI:
            if (record->event.pressed) {
                rgb_matrix_increase_hue_noeeprom();
            }
            return false;

        case RGB_HUD:
            if (record->event.pressed) {
                rgb_matrix_decrease_hue_noeeprom();
            }
            return false;

        case RGB_SPI:
            if (record->event.pressed) {
                rgb_matrix_increase_speed_noeeprom();
            }
            return false;

        case RGB_SPD:
            if (record->event.pressed) {
                rgb_matrix_decrease_speed_noeeprom();
            }
            return false;

        case RGB_SAI:
            if (record->event.pressed) {
                rgb_matrix_increase_sat_noeeprom();
            }
            return false;

       case RGB_SAD:
            if (record->event.pressed) {
                rgb_matrix_decrease_sat_noeeprom();
            }
            return false;

       case RGB_M_P:
            if (record->event.pressed) {
                rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_COLOR);
            }
            return false;

       case RGB_TOG:
            if (record->event.pressed) {
                rgb_matrix_toggle_noeeprom();
            }
            return false;

        case DEV_RESET:
            if (record->event.pressed) {
                f_dev_reset_press = 1;
                break_all_key();
            } else {
                f_dev_reset_press = 0;
            }
            return false;

        case SLEEP_MODE:
            if (record->event.pressed) {
                user_config.sleep_mode = (user_config.sleep_mode + 1) % 3;
                link_timeout           = user_config.sleep_mode == 1 ? (T_MIN * 1) : (T_MIN * 2);
                if (user_config.sleep_mode > 0) {
                    uint8_t temp_sleep = user_config.light_sleep;
                    user_config.light_sleep = user_config.alt_light_sleep;
                    user_config.alt_light_sleep = temp_sleep;
                }
                sleep_show_timer = timer_read32();
            }
            return false;

        case BAT_SHOW:
            if (record->event.pressed) {
                f_bat_hold = !f_bat_hold;
            }
            return false;

        case BAT_NUM:
            f_bat_num_show = record->event.pressed;
            if (!record->event.pressed) {
                for (uint8_t i = 1; i <= 10; i++) {
                    rgb_matrix_set_color(i, RGB_OFF);
                }
                rgb_matrix_update_pwm_buffers();
            }
            return false;

        case RGB_TEST:
            f_rgb_test_press = record->event.pressed;
            return false;

        case NUMLOCK_INS:
            if (record->event.pressed) {
                f_numlock_press = 1;
                if (get_mods() & MOD_MASK_CSA) {
                    tap_code(KC_INS);
                    f_numlock_press = 0;
                }
            } else {
                if (f_numlock_press) {
                    f_numlock_press = 0;
                    tap_code(KC_INS);
                }
            }
            return false;

        case NUMLOCK_IND:
            if (record->event.pressed) {
                if (user_config.numlock_state < 2 - game_mode_enable) { user_config.numlock_state++; }
                else { user_config.numlock_state = 0; }
            }
            return false;

        case SLEEP_NOW:
            if (USB_ACTIVE) { return false; }
            if (record->event.pressed) {
                wait_ms(100);
            } else {
                if (user_config.sleep_mode == 0) { return true; }
                f_goto_sleep = 1;
                f_goto_deepsleep = 1;
                no_act_time = 100;
                break_all_key();
            }
            return false;

        case DEBOUNCE_D:
        case DEBOUNCE_I:
            if (record->event.pressed) {
                uint8_t dir = keycode == DEBOUNCE_D ? 0 : 1;
                user_config.debounce_ms = step_helper(dir, user_config.debounce_ms);
#ifndef NO_DEBUG
                dprintf("debounce:      %dms\n", user_config.debounce_ms);
#endif
            }
            return false;

        case DEBOUNCE_T:
            if (record->event.pressed) {
                debounce_type();
            }
            return false;

        case SLEEP_D:
        case SLEEP_I:
            if (user_config.sleep_mode == 0) { return true; }
            if (record->event.pressed) {
                uint8_t dir = keycode == SLEEP_D ? 0 : 1;
                user_config.light_sleep  = step_helper(dir, user_config.light_sleep);
#ifndef NO_DEBUG
                dprintf("light sleep time:    %dmin\n", user_config.light_sleep);
#endif
            }
            return false;

        case SOCD_TOG:
            if (record->event.pressed) {
                break_all_key();
                user_config.socd_mode = (user_config.socd_mode + 1) % 4;
#ifndef NO_DEBUG
                dprintf("SOCD:    %s(%d)\n", socd_type[user_config.socd_mode], user_config.socd_mode);
#endif
                signal_rgb_led(user_config.socd_mode * 2, 0, led_idx.SOCD_TOG, UINT8_MAX, 3000);
            }
            return false;

        default:
            return true;
    }
    return true;
}

void post_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
#ifndef NO_DEBUG
        case DB_TOGG:
            dprintf("Keyboard: %s @ QMK: %s | BUILD: %s (%s)\n", QMK_KEYBOARD, QMK_VERSION, QMK_BUILDDATE, QMK_GIT_HASH);
            break;
#endif

        default:
            break;
    }
}

bool rgb_matrix_indicators_kb(void) {
    if (!rgb_matrix_indicators_user()) {
        return false;
    }

    if (f_bat_num_show) {
        bat_num_led();
    }

    power_save();

    // power down unused LEDs
    led_power_handle();
    return true;
}

/* qmk keyboard post init */
void keyboard_post_init_kb(void) {
    gpio_init();
    mcu_timer6_init();
    rf_uart_init();
    reset_led_idx();
    wait_ms(500);
    rf_device_init();

    break_all_key();
    load_eeprom_data();
    dial_sw_fast_scan();
#ifndef NO_DEBUG
    debug_enable   = false;
    // debug_matrix   = true;
    // debug_keyboard = true;
    // debug_mouse    = true;
#endif
    keyboard_post_init_user();
}

/*
void rgb_process_record_helper(const rgb_func_pointer rgb_func_noeeprom) {
    rgb_func_noeeprom();
    eeprom_update_timer = 0;
    rgb_update = 1;
}
*/

/* qmk housekeeping task */
void housekeeping_task_kb(void) {
    timer_pro();

    uart_receive_pro();

    uart_send_report_repeat();

    dev_sts_sync();

    custom_key_press();

    led_show();

#ifndef NO_DEBUG
    user_debug();
#endif

    delay_update_eeprom_data();

    if (game_mode_enable) { return; }

    sleep_handle();

    idle_enter_sleep();

}

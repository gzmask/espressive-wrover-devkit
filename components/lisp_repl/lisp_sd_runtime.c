#include "lisp_sd_runtime.h"
#include "board_sdcard.h"
#include "lisp_runtime.h"
#include "lispbm.h"
#include <string.h>

static lbm_value constant_symbol(const char *name)
{
    lbm_uint symbol;
    return lbm_add_symbol_const(name, &symbol) ? lbm_enc_sym(symbol) : ENC_SYM_MERROR;
}
static lbm_value sd_mount(lbm_value *arguments, lbm_uint count)
{
    (void)arguments;
    if (count != 0)
        return ENC_SYM_TERROR;
    return board_sdcard_request_mount() ? ENC_SYM_TRUE : ENC_SYM_EERROR;
}
static lbm_value sd_unmount(lbm_value *arguments, lbm_uint count)
{
    (void)arguments;
    if (count != 0)
        return ENC_SYM_TERROR;
    return board_sdcard_request_unmount() ? ENC_SYM_TRUE : ENC_SYM_EERROR;
}
static lbm_value sd_status(lbm_value *arguments, lbm_uint count)
{
    (void)arguments;
    if (count != 0)
        return ENC_SYM_TERROR;
    const char *name;
    switch (board_sdcard_state()) {
    case BOARD_SDCARD_DISABLED:
        name = "disabled";
        break;
    case BOARD_SDCARD_UNMOUNTED:
        name = "unmounted";
        break;
    case BOARD_SDCARD_MOUNTING:
        name = "mounting";
        break;
    case BOARD_SDCARD_MOUNTED:
        name = "mounted";
        break;
    case BOARD_SDCARD_READING:
        name = "reading";
        break;
    case BOARD_SDCARD_WRITING:
        name = "writing";
        break;
    case BOARD_SDCARD_READ_READY:
        name = "read-ready";
        break;
    case BOARD_SDCARD_WRITE_DONE:
        name = "write-done";
        break;
    case BOARD_SDCARD_OPERATION_ERROR:
        name = "operation-error";
        break;
    case BOARD_SDCARD_UNMOUNTING:
        name = "unmounting";
        break;
    case BOARD_SDCARD_FAULT:
        name = "fault";
        break;
    default:
        name = "fault";
        break;
    }
    return constant_symbol(name);
}
static bool string_argument(lbm_value value, char **data, size_t *length)
{
    size_t encoded;
    if (!lbm_dec_str_size(value, data, &encoded) || !*data || encoded == 0 || (*data)[encoded - 1] != 0 ||
        memchr(*data, 0, encoded - 1) != NULL)
        return false;
    *length = encoded - 1;
    return true;
}
static lbm_value sd_read(lbm_value *arguments, lbm_uint count)
{
    char *name;
    size_t length;
    if (count != 1 || !string_argument(arguments[0], &name, &length) ||
        !board_sdcard_filename_valid(name, length))
        return ENC_SYM_TERROR;
    return board_sdcard_request_read(name, length) ? ENC_SYM_TRUE : ENC_SYM_EERROR;
}
static lbm_value sd_write(lbm_value *arguments, lbm_uint count)
{
    char *name, *text;
    size_t name_length, text_length;
    if (count != 2 || !string_argument(arguments[0], &name, &name_length) ||
        !string_argument(arguments[1], &text, &text_length) ||
        !board_sdcard_filename_valid(name, name_length) || !board_sdcard_text_valid(text, text_length))
        return ENC_SYM_TERROR;
    return board_sdcard_request_write(name, name_length, text, text_length) ? ENC_SYM_TRUE : ENC_SYM_EERROR;
}
static lbm_value sd_result(lbm_value *arguments, lbm_uint count)
{
    (void)arguments;
    if (count != 0)
        return ENC_SYM_TERROR;
    board_sdcard_state_t state = board_sdcard_state();
    if (state == BOARD_SDCARD_READ_READY) {
        size_t length = board_sdcard_read_result_size();
        lbm_value result;
        if (!lbm_create_array(&result, (lbm_uint)(length + 1)))
            return ENC_SYM_MERROR;
        char *text = lbm_dec_str(result);
        size_t copied;
        if (!text || !board_sdcard_take_read_result(text, length + 1, &copied) || copied != length)
            return ENC_SYM_EERROR;
        text[length] = 0;
        return result;
    }
    if (state == BOARD_SDCARD_WRITE_DONE || state == BOARD_SDCARD_OPERATION_ERROR) {
        board_sdcard_state_t completed;
        if (!board_sdcard_take_operation_result(&completed))
            return ENC_SYM_EERROR;
        return completed == BOARD_SDCARD_WRITE_DONE ? ENC_SYM_TRUE : constant_symbol("error");
    }
    switch (state) {
    case BOARD_SDCARD_MOUNTING:
    case BOARD_SDCARD_READING:
    case BOARD_SDCARD_WRITING:
    case BOARD_SDCARD_UNMOUNTING:
        return constant_symbol("pending");
    case BOARD_SDCARD_FAULT:
        return constant_symbol("error");
    default:
        return ENC_SYM_NIL;
    }
}
static lbm_value sd_last_error(lbm_value *arguments, lbm_uint count)
{
    (void)arguments;
    if (count != 0)
        return ENC_SYM_TERROR;
    return lbm_enc_i32((int32_t)board_sdcard_last_error());
}
static lbm_value sd_diagnostics(lbm_value *arguments, lbm_uint count)
{
    (void)arguments;
    if (count != 0)
        return ENC_SYM_TERROR;
    board_sdcard_diagnostic_t diagnostic;
    board_sdcard_get_diagnostics(&diagnostic);
    char text[BOARD_SDCARD_DIAGNOSTIC_MAX];
    if (!board_sdcard_diagnostic_format(&diagnostic, text, sizeof(text)))
        return ENC_SYM_EERROR;
    size_t length = strlen(text) + 1;
    lbm_value result;
    if (!lbm_create_array(&result, (lbm_uint)length))
        return ENC_SYM_MERROR;
    char *data = lbm_dec_str(result);
    if (!data)
        return ENC_SYM_EERROR;
    memcpy(data, text, length);
    return result;
}
bool lisp_sd_runtime_register(void)
{
    return lbm_add_extension("sd-mount", sd_mount) && lbm_add_extension("sd-unmount", sd_unmount) &&
           lbm_add_extension("sd-status", sd_status) && lbm_add_extension("sd-read", sd_read) &&
           lbm_add_extension("sd-write", sd_write) && lbm_add_extension("sd-result", sd_result) &&
           lbm_add_extension("sd-last-error", sd_last_error) &&
           lbm_add_extension("sd-diagnostics", sd_diagnostics);
}

#include "lisp_lcd_runtime.h"
#include "lisp_runtime.h"
#include "lispbm.h"

static display_lifecycle_backend_t backend;
static bool attached;
bool lisp_runtime_lcd_attach(const display_lifecycle_backend_t *value)
{
    if (!value || !value->request || !value->status)
        return false;
    backend = *value;
    attached = true;
    return true;
}
static lbm_value lcd_request(lbm_uint count, bool on)
{
    if (count != 0)
        return ENC_SYM_TERROR;
    if (!attached || lisp_runtime_app_active() || lisp_runtime_load_active() ||
        !backend.request(backend.user, on))
        return ENC_SYM_EERROR;
    return ENC_SYM_TRUE;
}
static lbm_value lcd_on(lbm_value *args, lbm_uint count)
{
    (void)args;
    return lcd_request(count, true);
}
static lbm_value lcd_off(lbm_value *args, lbm_uint count)
{
    (void)args;
    return lcd_request(count, false);
}
static lbm_value lcd_status(lbm_value *args, lbm_uint count)
{
    (void)args;
    if (count != 0)
        return ENC_SYM_TERROR;
    const char *name = "disabled";
    if (attached) {
        switch (backend.status(backend.user)) {
        case DISPLAY_STARTING:
            name = "starting";
            break;
        case DISPLAY_ON:
            name = "on";
            break;
        case DISPLAY_STOPPING:
            name = "stopping";
            break;
        case DISPLAY_OFF:
            name = "off";
            break;
        case DISPLAY_SD_BLOCKED:
            name = "sd-blocked";
            break;
        case DISPLAY_FAULT:
            name = "fault";
            break;
        default:
            name = "fault";
            break;
        }
    }
    lbm_uint symbol;
    return lbm_add_symbol_const(name, &symbol) ? lbm_enc_sym(symbol) : ENC_SYM_MERROR;
}
bool lisp_lcd_runtime_register(void)
{
    return lbm_add_extension("lcd-on", lcd_on) && lbm_add_extension("lcd-off", lcd_off) &&
           lbm_add_extension("lcd-status", lcd_status);
}

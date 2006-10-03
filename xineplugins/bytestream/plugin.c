#include <xine.h>
#include <xine/compat.h>
#include <xine/input_plugin.h>
#include <xine/xine_internal.h>
#include <xine/xineutils.h>

extern void *init_plugin (xine_t *xine, void *data);
/*
 * exported plugin catalog entry
 */
const plugin_info_t kbytestream_xine_plugin_info[] = {
    /* type, API, "name", version, special_info, init_function */
    { PLUGIN_INPUT, 17, "KBYTESTREAM", XINE_VERSION_CODE, NULL, init_plugin },
    { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};

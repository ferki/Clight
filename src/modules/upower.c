#include <bus.h>

static int upower_check(void);
static int upower_init(void);
static int on_upower_change(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

static sd_bus_slot *slot;
static upower_upd upower_msg = { UPOWER_UPD };

const char *up_topic = "AcState";

MODULE("UPOWER");

static void init(void) {
    if (upower_init() != 0) {
        WARN("UPOWER: Failed to init.\n");
        m_poisonpill(self());
    }
}

static bool check(void) {
    return true;
}

static bool evaluate(void) {
    /* Start as soon as upower becomes available */
    return upower_check() == 0;
}

static void receive(const msg_t *const msg, const void* userdata) {
    
}

static void destroy(void) {
    /* Destroy this match slot */
    if (slot) {
        slot = sd_bus_slot_unref(slot);
    }
}

static int upower_check(void) {
    /* check initial AC state */
    SYSBUS_ARG(args, "org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery");
    int r = get_property(&args, "b", &state.ac_state, sizeof(state.ac_state));
    if (r < 0 && state.ac_state == -1) {
        /* Upower not available, for now. Let's assume ON_AC! */
        state.ac_state = ON_AC;
    } else {
        INFO("UPOWER: Initial AC state: %s.\n", state.ac_state == ON_AC ? "connected" : "disconnected");
    }
    return -(r < 0);
}

static int upower_init(void) {
    SYSBUS_ARG(args, "org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.DBus.Properties", "PropertiesChanged");
    return add_match(&args, &slot, on_upower_change);
}

/*
 * Callback on upower changes: recheck on_battery boolean value
 */
static int on_upower_change(__attribute__((unused)) sd_bus_message *m, void *userdata, __attribute__((unused)) sd_bus_error *ret_error) {
    SYSBUS_ARG(args, "org.freedesktop.UPower",  "/org/freedesktop/UPower", "org.freedesktop.UPower", "OnBattery");

    /*
     * Store last ac_state in old struct to be matched against new one
     * as we cannot be sure that a OnBattery changed signal has been really sent:
     * our match will receive these signals:
     * .DaemonVersion                      property  s         "0.99.5"     emits-change
     * .LidIsClosed                        property  b         true         emits-change
     * .LidIsPresent                       property  b         true         emits-change
     * .OnBattery                          property  b         false        emits-change
     */
    int old_ac_state = state.ac_state;
    int r = get_property(&args, "b", &state.ac_state, sizeof(state.ac_state));
    if (!r && old_ac_state != state.ac_state) {
        INFO(state.ac_state ? "UPOWER: AC cable disconnected. Powersaving mode enabled.\n" : "UPOWER: Ac cable connected. Powersaving mode disabled.\n");
        upower_msg.old = old_ac_state;
        upower_msg.new = state.ac_state;
        M_PUB(up_topic, &upower_msg);
    }
    return 0;
}

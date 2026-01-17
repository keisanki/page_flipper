#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>

#define TAG "PageFlipper"

#define HID_KEYBOARD_UP_ARROW 0x52
#define HID_KEYBOARD_DOWN_ARROW 0x51
#define HID_KEYBOARD_LEFT_ARROW 0x50
#define HID_KEYBOARD_RIGHT_ARROW 0x4F

typedef enum {
    PageFlipperViewMain,
} PageFlipperViewId;

typedef enum {
    PageFlipperEventA7Press,
    PageFlipperEventA7DoublePress,
    PageFlipperEventA6Press,
} PageFlipperCustomEvent;

typedef struct {
    bool connected;
    uint16_t last_hid_key;
    uint32_t last_press_timestamp;
} PageFlipperModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Bt* bt;
    View* main_view;
    FuriHalBleProfileBase* ble_profile;
    FuriTimer* timer;
} PageFlipperApp;

static void page_flipper_draw_callback(Canvas* canvas, void* model) {
    PageFlipperModel* my_model = model;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Page Flipper");

    if(!my_model->connected) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Connect to PageFlipper...");
    } else {
        uint32_t now = furi_get_tick();
        bool flashing = (now - my_model->last_press_timestamp < 200);
        uint16_t key = flashing ? my_model->last_hid_key : 0;

        // Draw arrow keys
        if(key == HID_KEYBOARD_UP_ARROW) canvas_draw_box(canvas, 60, 20, 8, 8);
        else canvas_draw_frame(canvas, 60, 20, 8, 8);

        if(key == HID_KEYBOARD_DOWN_ARROW) canvas_draw_box(canvas, 60, 40, 8, 8);
        else canvas_draw_frame(canvas, 60, 40, 8, 8);

        if(key == HID_KEYBOARD_LEFT_ARROW) canvas_draw_box(canvas, 45, 30, 8, 8);
        else canvas_draw_frame(canvas, 45, 30, 8, 8);

        if(key == HID_KEYBOARD_RIGHT_ARROW) canvas_draw_box(canvas, 75, 30, 8, 8);
        else canvas_draw_frame(canvas, 75, 30, 8, 8);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 0, 64, AlignLeft, AlignBottom, "OK: Help");
    canvas_draw_str_aligned(canvas, 128, 64, AlignRight, AlignBottom, "Back: Exit");
}

static void page_flipper_send_key(PageFlipperApp* app, uint16_t hid_key) {
    if(!app->ble_profile) return;
    ble_profile_hid_kb_press(app->ble_profile, hid_key);
    furi_delay_ms(20);
    ble_profile_hid_kb_release(app->ble_profile, hid_key);

    with_view_model(
        app->main_view,
        PageFlipperModel * model,
        {
            model->last_hid_key = hid_key;
            model->last_press_timestamp = furi_get_tick();
        },
        true);
}

static void page_flipper_bt_status_callback(BtStatus status, void* context) {
    PageFlipperApp* app = context;
    bool connected = (status == BtStatusConnected);
    with_view_model(
        app->main_view,
        PageFlipperModel * model,
        {
            model->connected = connected;
        },
        true);
}

static bool page_flipper_input_callback(InputEvent* event, void* context) {
    PageFlipperApp* app = context;
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        } else if(event->key == InputKeyOk) {
            // TODO: Help
            return true;
        } else {
            uint16_t hid_key = 0;
            if(event->key == InputKeyUp) hid_key = HID_KEYBOARD_UP_ARROW;
            else if(event->key == InputKeyDown) hid_key = HID_KEYBOARD_DOWN_ARROW;
            else if(event->key == InputKeyLeft) hid_key = HID_KEYBOARD_LEFT_ARROW;
            else if(event->key == InputKeyRight) hid_key = HID_KEYBOARD_RIGHT_ARROW;

            if(hid_key) {
                page_flipper_send_key(app, hid_key);
                return true;
            }
        }
    }
    return false;
}

static bool page_flipper_custom_event_callback(void* context, uint32_t event) {
    PageFlipperApp* app = context;
    if(event == PageFlipperEventA7Press) {
        page_flipper_send_key(app, HID_KEYBOARD_RIGHT_ARROW);
        return true;
    } else if(event == PageFlipperEventA7DoublePress) {
        page_flipper_send_key(app, HID_KEYBOARD_LEFT_ARROW);
        return true;
    } else if(event == PageFlipperEventA6Press) {
        page_flipper_send_key(app, HID_KEYBOARD_LEFT_ARROW);
        return true;
    }
    return false;
}

static void page_flipper_timer_callback(void* context) {
    PageFlipperApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PageFlipperEventA7Press);
}

static void page_flipper_gpio_a7_callback(void* context) {
    PageFlipperApp* app = context;
    if(furi_timer_is_running(app->timer)) {
        furi_timer_stop(app->timer);
        view_dispatcher_send_custom_event(app->view_dispatcher, PageFlipperEventA7DoublePress);
    } else {
        furi_timer_start(app->timer, 300);
    }
}

static void page_flipper_gpio_a6_callback(void* context) {
    PageFlipperApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PageFlipperEventA6Press);
}

PageFlipperApp* page_flipper_app_alloc() {
    PageFlipperApp* app = malloc(sizeof(PageFlipperApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->bt = furi_record_open(RECORD_BT);
    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, page_flipper_custom_event_callback);

    app->main_view = view_alloc();
    view_allocate_model(app->main_view, ViewModelTypeLockFree, sizeof(PageFlipperModel));
    view_set_draw_callback(app->main_view, page_flipper_draw_callback);
    view_set_input_callback(app->main_view, page_flipper_input_callback);
    view_set_context(app->main_view, app);

    view_dispatcher_add_view(app->view_dispatcher, PageFlipperViewMain, app->main_view);
    view_dispatcher_switch_to_view(app->view_dispatcher, PageFlipperViewMain);

    // Initialize BT
    bt_disconnect(app->bt);
    furi_delay_ms(200);
    bt_keys_storage_set_default_path(app->bt);
    app->ble_profile = bt_profile_start(app->bt, ble_profile_hid, NULL);
    bt_set_status_changed_callback(app->bt, page_flipper_bt_status_callback, app);

    // Initialize Timer
    app->timer = furi_timer_alloc(page_flipper_timer_callback, FuriTimerTypeOnce, app);

    // Initialize GPIOs
    furi_hal_gpio_init_ex(&gpio_ext_pa7, GpioModeInterruptFall, GpioPullUp, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_add_int_callback(&gpio_ext_pa7, page_flipper_gpio_a7_callback, app);

    furi_hal_gpio_init_ex(&gpio_ext_pa6, GpioModeInterruptFall, GpioPullUp, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_add_int_callback(&gpio_ext_pa6, page_flipper_gpio_a6_callback, app);

    return app;
}

void page_flipper_app_free(PageFlipperApp* app) {
    furi_assert(app);

    // Free GPIOs
    furi_hal_gpio_remove_int_callback(&gpio_ext_pa7);
    furi_hal_gpio_init(&gpio_ext_pa7, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_remove_int_callback(&gpio_ext_pa6);
    furi_hal_gpio_init(&gpio_ext_pa6, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    // Free Timer
    furi_timer_free(app->timer);

    bt_set_status_changed_callback(app->bt, NULL, NULL);
    bt_profile_restore_default(app->bt);

    view_dispatcher_remove_view(app->view_dispatcher, PageFlipperViewMain);
    view_free(app->main_view);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_BT);
    free(app);
}

int32_t page_flipper_app(void* p) {
    UNUSED(p);
    PageFlipperApp* app = page_flipper_app_alloc();

    FURI_LOG_I(TAG, "Page Flipper started");

    view_dispatcher_run(app->view_dispatcher);

    page_flipper_app_free(app);
    return 0;
}

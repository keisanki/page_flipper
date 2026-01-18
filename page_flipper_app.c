#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>
#include <string.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <page_flipper_icons.h>

#define TAG "PageFlipper"

#define HID_KEYBOARD_UP_ARROW 0x52
#define HID_KEYBOARD_DOWN_ARROW 0x51
#define HID_KEYBOARD_LEFT_ARROW 0x50
#define HID_KEYBOARD_RIGHT_ARROW 0x4F

typedef enum {
    PageFlipperViewMain,
    PageFlipperViewHelp,
} PageFlipperViewId;

typedef enum {
    PageFlipperEventA7Press,
    PageFlipperEventA7DoublePress,
    PageFlipperEventA6Press,
} PageFlipperCustomEvent;

typedef struct {
    bool connected;
    bool started;
    uint16_t last_hid_key;
    uint32_t last_press_timestamp;
} PageFlipperModel;

typedef struct {
    uint8_t page;
} HelpModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Bt* bt;
    View* main_view;
    View* help_view;
    FuriHalBleProfileBase* ble_profile;
    BleProfileHidParams ble_params;
    FuriThread* worker_thread;
    bool running;
    FuriTimer* flash_timer;
} PageFlipperApp;

static void page_flipper_flash_timer_callback(void* context) {
    PageFlipperApp* app = context;
    with_view_model(app->main_view, PageFlipperModel * model, { UNUSED(model); }, true);
}

static int32_t page_flipper_worker(void* context) {
    PageFlipperApp* app = context;
    bool last_pa6 = true;
    bool last_pa7 = true;
    uint32_t last_pa7_press = 0;

    while(app->running) {
        bool pa6 = furi_hal_gpio_read(&gpio_ext_pa6);
        bool pa7 = furi_hal_gpio_read(&gpio_ext_pa7);

        if(!pa6 && last_pa6) {
            view_dispatcher_send_custom_event(app->view_dispatcher, PageFlipperEventA6Press);
        }

        if(!pa7 && last_pa7) {
            uint32_t now = furi_get_tick();
            if(now - last_pa7_press < 300) {
                view_dispatcher_send_custom_event(app->view_dispatcher, PageFlipperEventA7DoublePress);
                last_pa7_press = 0;
            } else {
                last_pa7_press = now;
            }
        } else if (last_pa7_press != 0 && furi_get_tick() - last_pa7_press > 300) {
             view_dispatcher_send_custom_event(app->view_dispatcher, PageFlipperEventA7Press);
             last_pa7_press = 0;
        }

        last_pa6 = pa6;
        last_pa7 = pa7;
        furi_delay_ms(10);
    }
    return 0;
}

static void page_flipper_help_draw_callback(Canvas* canvas, void* model) {
    HelpModel* my_model = model;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Help");

    canvas_set_font(canvas, FontSecondary);
    if(my_model->page == 0) {
        canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, "Foot pedal (A7 to GND):");
        canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignTop, "Single: Page Forward");
        canvas_draw_str_aligned(canvas, 64, 39, AlignCenter, AlignTop, "Double: Page Backward");
    } else if(my_model->page == 1) {
        canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, "Foot pedal (A6 to GND):");
        canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignTop, "Single: Page Backward");
    } else if(my_model->page == 2) {
        canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, "Keypad:");
        canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignTop, "Arrows: Send keys");
    }

    // Separator line
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, 52, 128, 52);

    // Page indicator
    for(int i = 0; i < 3; i++) {
        if(i == my_model->page) {
            canvas_draw_disc(canvas, 54 + i * 10, 58, 2);
        } else {
            canvas_draw_circle(canvas, 54 + i * 10, 58, 2);
        }
    }

    canvas_draw_icon(canvas, 0, 55, &I_Pin_back_arrow_10x8);
    canvas_draw_str(canvas, 13, 62, ": Exit");
}

static bool page_flipper_help_input_callback(InputEvent* event, void* context) {
    PageFlipperApp* app = context;
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            view_dispatcher_switch_to_view(app->view_dispatcher, PageFlipperViewMain);
            return true;
        } else if(event->key == InputKeyLeft) {
            with_view_model(
                app->help_view,
                HelpModel * model,
                {
                    if(model->page > 0) model->page--;
                },
                true);
            return true;
        } else if(event->key == InputKeyRight) {
            with_view_model(
                app->help_view,
                HelpModel * model,
                {
                    if(model->page < 2) model->page++;
                },
                true);
            return true;
        }
    }
    return false;
}

static void page_flipper_draw_callback(Canvas* canvas, void* model) {
    PageFlipperModel* my_model = model;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Page Flipper");

    uint32_t now = furi_get_tick();
    bool flashing = (now - my_model->last_press_timestamp < 200);
    uint16_t key = flashing ? my_model->last_hid_key : 0;

    // Left Arrow Box
    canvas_set_color(canvas, ColorBlack);
    if(key == HID_KEYBOARD_LEFT_ARROW) {
        canvas_draw_rbox(canvas, 10, 16, 35, 30, 5);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, 10, 16, 35, 30, 5);
    }
    // Draw Left Arrow
    canvas_draw_line(canvas, 18, 30, 37, 30);
    canvas_draw_line(canvas, 18, 31, 37, 31);
    canvas_draw_line(canvas, 18, 30, 25, 25);
    canvas_draw_line(canvas, 18, 31, 25, 26);
    canvas_draw_line(canvas, 18, 30, 25, 35);
    canvas_draw_line(canvas, 18, 31, 25, 36);
    
    // Right Arrow Box
    canvas_set_color(canvas, ColorBlack);
    if(key == HID_KEYBOARD_RIGHT_ARROW) {
        canvas_draw_rbox(canvas, 83, 16, 35, 30, 5);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, 83, 16, 35, 30, 5);
    }
    // Draw Right Arrow
    canvas_draw_line(canvas, 91, 30, 110, 30);
    canvas_draw_line(canvas, 91, 31, 110, 31);
    canvas_draw_line(canvas, 110, 30, 103, 25);
    canvas_draw_line(canvas, 110, 31, 103, 26);
    canvas_draw_line(canvas, 110, 30, 103, 35);
    canvas_draw_line(canvas, 110, 31, 103, 36);

    // Up Arrow Box
    canvas_set_color(canvas, ColorBlack);
    if(key == HID_KEYBOARD_UP_ARROW) {
        canvas_draw_rbox(canvas, 48, 16, 32, 14, 3);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, 48, 16, 32, 14, 3);
    }
    // Draw Up Arrow
    canvas_draw_line(canvas, 63, 19, 63, 27);
    canvas_draw_line(canvas, 64, 19, 64, 27);
    canvas_draw_line(canvas, 63, 19, 59, 23);
    canvas_draw_line(canvas, 64, 19, 60, 23);
    canvas_draw_line(canvas, 63, 19, 67, 23);
    canvas_draw_line(canvas, 64, 19, 68, 23);

    // Down Arrow Box
    canvas_set_color(canvas, ColorBlack);
    if(key == HID_KEYBOARD_DOWN_ARROW) {
        canvas_draw_rbox(canvas, 48, 32, 32, 14, 3);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, 48, 32, 32, 14, 3);
    }
    // Draw Down Arrow
    canvas_draw_line(canvas, 63, 35, 63, 43);
    canvas_draw_line(canvas, 64, 35, 64, 43);
    canvas_draw_line(canvas, 63, 43, 59, 39);
    canvas_draw_line(canvas, 64, 43, 60, 39);
    canvas_draw_line(canvas, 63, 43, 67, 39);
    canvas_draw_line(canvas, 64, 43, 68, 39);
    
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, 52, 128, 52);

    canvas_set_font(canvas, FontSecondary);
    if(!my_model->started) {
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "Connect to PageFlip...");
    } else {
        if (my_model->connected) {
             canvas_draw_icon(canvas, 15, 54, &I_Ok_btn_9x9);
             canvas_draw_str(canvas, 26, 62, ": Help");
             
             canvas_draw_icon(canvas, 75, 55, &I_Pin_back_arrow_10x8);
             canvas_draw_str(canvas, 88, 62, ": Exit");
        } else {
             canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "BLE not connected.");
        }
    }
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
            model->started = true;
        },
        true);
    
    furi_timer_start(app->flash_timer, 200);
}

static void page_flipper_bt_status_callback(BtStatus status, void* context) {
    PageFlipperApp* app = context;
    bool connected = (status == BtStatusConnected);
    with_view_model(
        app->main_view,
        PageFlipperModel * model,
        {
            model->connected = connected;
            if(connected) model->started = true;
        },
        true);
}

static bool page_flipper_input_callback(InputEvent* event, void* context) {
    PageFlipperApp* app = context;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk) {
            view_dispatcher_switch_to_view(app->view_dispatcher, PageFlipperViewHelp);
            return true;
        } else if(event->key == InputKeyBack) {
            view_dispatcher_stop(app->view_dispatcher);
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

PageFlipperApp* page_flipper_app_alloc() {
    PageFlipperApp* app = malloc(sizeof(PageFlipperApp));
    memset(app, 0, sizeof(PageFlipperApp));

    // Initialize GPIOs as Input (No interrupts)
    furi_hal_gpio_init(&gpio_ext_pa6, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_ext_pa7, GpioModeInput, GpioPullUp, GpioSpeedLow);

    app->gui = furi_record_open(RECORD_GUI);
    app->bt = furi_record_open(RECORD_BT);
    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, page_flipper_custom_event_callback);

    app->main_view = view_alloc();
    view_allocate_model(app->main_view, ViewModelTypeLocking, sizeof(PageFlipperModel));
    view_set_draw_callback(app->main_view, page_flipper_draw_callback);
    view_set_input_callback(app->main_view, page_flipper_input_callback);
    view_set_context(app->main_view, app);

    app->help_view = view_alloc();
    view_allocate_model(app->help_view, ViewModelTypeLocking, sizeof(HelpModel));
    view_set_draw_callback(app->help_view, page_flipper_help_draw_callback);
    view_set_input_callback(app->help_view, page_flipper_help_input_callback);
    view_set_context(app->help_view, app);

    view_dispatcher_add_view(app->view_dispatcher, PageFlipperViewMain, app->main_view);
    view_dispatcher_add_view(app->view_dispatcher, PageFlipperViewHelp, app->help_view);
    view_dispatcher_switch_to_view(app->view_dispatcher, PageFlipperViewMain);

    // Initialize BT
    bt_disconnect(app->bt);
    furi_delay_ms(200);
    bt_keys_storage_set_default_path(app->bt);
    
    app->ble_params.device_name_prefix = "PageFlip";
    app->ble_params.mac_xor = 0x0000;
    
    if (ble_profile_hid != NULL) {
         app->ble_profile = bt_profile_start(app->bt, ble_profile_hid, &app->ble_params);
         furi_delay_ms(500); 
         bt_set_status_changed_callback(app->bt, page_flipper_bt_status_callback, app);
    }

    // Start worker thread
    app->running = true;
    app->worker_thread = furi_thread_alloc_ex("PageFlipperWorker", 1024, page_flipper_worker, app);
    furi_thread_start(app->worker_thread);

    // Initialize flash timer
    app->flash_timer = furi_timer_alloc(page_flipper_flash_timer_callback, FuriTimerTypeOnce, app);

    return app;
}

void page_flipper_app_free(PageFlipperApp* app) {
    furi_assert(app);

    furi_timer_stop(app->flash_timer);
    furi_timer_free(app->flash_timer);

    app->running = false;
    furi_thread_join(app->worker_thread);
    furi_thread_free(app->worker_thread);

    bt_set_status_changed_callback(app->bt, NULL, NULL);
    bt_profile_restore_default(app->bt);

    view_dispatcher_remove_view(app->view_dispatcher, PageFlipperViewMain);
    view_dispatcher_remove_view(app->view_dispatcher, PageFlipperViewHelp);
    view_free(app->main_view);
    view_free(app->help_view);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_BT);

    furi_hal_gpio_init(&gpio_ext_pa6, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_ext_pa7, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    free(app);
}

int32_t page_flipper_app(void* p) {
    UNUSED(p);
    PageFlipperApp* app = page_flipper_app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    page_flipper_app_free(app);
    return 0;
}
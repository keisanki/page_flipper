#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>

#define TAG "PageFlipper"

typedef enum {
    PageFlipperViewMain,
} PageFlipperViewId;

typedef struct {
    bool connected;
    InputKey last_key;
    bool key_pressed;
} PageFlipperModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Bt* bt;
    View* main_view;
    FuriHalBleProfileBase* ble_profile;
} PageFlipperApp;

static void page_flipper_draw_callback(Canvas* canvas, void* model) {
    PageFlipperModel* my_model = model;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Page Flipper");

    if(!my_model->connected) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Connect to PageFlipper...");
    } else {
        // Draw arrow keys
        canvas_draw_frame(canvas, 60, 20, 8, 8); // Up
        canvas_draw_frame(canvas, 60, 40, 8, 8); // Down
        canvas_draw_frame(canvas, 45, 30, 8, 8); // Left
        canvas_draw_frame(canvas, 75, 30, 8, 8); // Right
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 0, 64, AlignLeft, AlignBottom, "OK: Help");
    canvas_draw_str_aligned(canvas, 128, 64, AlignRight, AlignBottom, "Back: Exit");
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
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
    // Handle other keys
    return false;
}

PageFlipperApp* page_flipper_app_alloc() {
    PageFlipperApp* app = malloc(sizeof(PageFlipperApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->bt = furi_record_open(RECORD_BT);
    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

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

    return app;
}

void page_flipper_app_free(PageFlipperApp* app) {
    furi_assert(app);
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

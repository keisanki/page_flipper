#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>

#define TAG "PageFlipper"

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Bt* bt;
} PageFlipperApp;

PageFlipperApp* page_flipper_app_alloc() {
    PageFlipperApp* app = malloc(sizeof(PageFlipperApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->bt = furi_record_open(RECORD_BT);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

void page_flipper_app_free(PageFlipperApp* app) {
    furi_assert(app);
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

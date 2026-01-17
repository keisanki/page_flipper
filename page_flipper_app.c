#include <furi.h>

int32_t page_flipper_app(void* p) {
    UNUSED(p);
    FURI_LOG_I("PageFlipper", "Hello, Page Flipper!");
    return 0;
}

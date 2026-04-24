// LVGL custom allocator: routes all widget/style memory to PSRAM.
//
// Root cause: malloc() on Arduino-ESP32 keeps small allocations (<threshold)
// in internal SRAM. LVGL creates ~170 widget/style objects during
// buildScreens(), each ~100-200 bytes — they all go to SRAM and consume
// the full ~55 KB heap, leaving 280 bytes free for SSL connections.
//
// Fix: redirect lv_malloc_core / lv_realloc_core / lv_free_core to
// heap_caps_malloc with MALLOC_CAP_SPIRAM so every LVGL alloc lands in the
// 8 MB PSRAM pool instead of the tiny fragmented SRAM heap.
//
// The DMA draw buffers are NOT affected — they are allocated explicitly with
// MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL in LVGLDisplayManager::initHardware().

#include <lvgl.h>
#include <esp_heap_caps.h>

extern "C" {

void lv_mem_init(void) {}
void lv_mem_deinit(void) {}

lv_mem_pool_t lv_mem_add_pool(void* mem, size_t bytes) {
    LV_UNUSED(mem);
    LV_UNUSED(bytes);
    return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool) {
    LV_UNUSED(pool);
}

void* lv_malloc_core(size_t size) {
    void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = malloc(size);  // fallback: SRAM if PSRAM somehow full
    return p;
}

void* lv_realloc_core(void* p, size_t new_size) {
    // heap_caps_realloc handles cross-heap moves (SRAM→PSRAM) safely.
    void* np = heap_caps_realloc(p, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!np) np = realloc(p, new_size);
    return np;
}

void lv_free_core(void* p) {
    free(p);  // ESP32 free() works for both SRAM and PSRAM pointers
}

void lv_mem_monitor_core(lv_mem_monitor_t* mon_p) {
    LV_UNUSED(mon_p);
}

lv_result_t lv_mem_test_core(void) {
    return LV_RESULT_OK;
}

}  // extern "C"

#include "st7701.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"
#include "micropython/modules/util.hpp"
#include <cstdio>
#include <cfloat>


#include "hardware/structs/ioqspi.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/xip_ctrl.h"


using namespace pimoroni;


extern "C" {
#include "presto.h"
#include "py/builtin.h"
#include <stdarg.h>

void __printf_debug_flush() {
    for(auto i = 0u; i < 10; i++) {
        sleep_ms(1);
        mp_event_handle_nowait();
    }
}

int mp_vprintf(const mp_print_t *print, const char *fmt, va_list args);

void presto_debug(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = mp_vprintf(&mp_plat_print, fmt, ap);
    va_end(ap);
    __printf_debug_flush();
    (void)ret;
}

/***** Variables Struct *****/
typedef struct _presto_obj_t {
    mp_obj_base_t base;
    ST7701* presto;
    uint16_t* next_fb;
    uint16_t* curr_fb;
} _presto_obj_t;

typedef struct _ModPicoGraphics_obj_t {
    mp_obj_base_t base;
    PicoGraphics *graphics;
    DisplayDriver *display;
} ModPicoGraphics_obj_t;



void presto_core1_entry() {
    multicore_fifo_push_blocking(0); // TODO: Remove, debug to signal core has actually started

    ST7701 *presto = (ST7701*)multicore_fifo_pop_blocking();

    presto->init();
    multicore_fifo_push_blocking(0); // Todo handle issues here?*/
    while (1) __wfe();
}

#define stack_size 4096u
static uint32_t core1_stack[stack_size] = {0};

mp_obj_t presto_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    _presto_obj_t *self = nullptr;

    enum { ARG_pio, ARG_sm, ARG_pins, ARG_common_pin, ARG_direction, ARG_counts_per_rev, ARG_count_microsteps, ARG_freq_divider };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_pio, MP_ARG_INT },
        { MP_QSTR_sm, MP_ARG_INT }
    };

    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    presto_debug("malloc self\n");
    self = mp_obj_malloc_with_finaliser(_presto_obj_t, &Presto_type);

    presto_debug("set fb pointers\n");
    self->curr_fb = m_new(uint16_t, 28800); // (uint16_t*)0x11000000;
    self->next_fb = self->curr_fb; //(uint16_t*)0x11080000;

    presto_debug("m_new_class(ST7701...\n");
    ST7701 *presto = m_new_class(ST7701, WIDTH, HEIGHT, ROTATE_0,
        SPIPins{spi1, LCD_CS, LCD_CLK, LCD_DAT, PIN_UNUSED, LCD_DC, BACKLIGHT},
        self->next_fb,
        LCD_D0);

    self->presto = presto;

    presto_debug("launch core1\n");
    multicore_reset_core1();
    //multicore_launch_core1(presto_core1_entry);
    // multicore_launch_core1 probably uses malloc for its stack, and will return but apparently not launch core1 on MicroPython
    multicore_launch_core1_with_stack(presto_core1_entry, core1_stack, stack_size);
    presto_debug("waiting for core1...\n");
    multicore_fifo_pop_blocking();
    presto_debug("core1 running...\n");

    presto_debug("signal core1\n");
    multicore_fifo_push_blocking((uintptr_t)self->presto);
    int res = multicore_fifo_pop_blocking();
    presto_debug("core1 returned\n");

    //presto_debug("presto->init(): ");
    //presto->init();
    //presto_debug("ok\n");

    if(res != 0) {
        mp_raise_msg(&mp_type_RuntimeError, "presto: failed to start ST7701 on Core1.");
    }

    return MP_OBJ_FROM_PTR(self);
}

extern mp_obj_t presto_update(mp_obj_t self_in, mp_obj_t graphics_in) {
    _presto_obj_t *self = MP_OBJ_TO_PTR2(self_in, _presto_obj_t);
    ModPicoGraphics_obj_t *picographics = MP_OBJ_TO_PTR2(graphics_in, ModPicoGraphics_obj_t);

    // Wait for any pending flip to finish
    self->presto->wait_for_vsync();

    self->presto->set_framebuffer(self->next_fb);

    // Flip the buffers
    std::swap(self->next_fb, self->curr_fb);

    // Make sure PicoGraphics is drawing into our current front buffer
    picographics->graphics->frame_buffer = (uint8_t *)self->next_fb;

    return mp_const_none;
}

mp_obj_t presto___del__(mp_obj_t self_in) {
    _presto_obj_t *self = MP_OBJ_TO_PTR2(self_in, _presto_obj_t);
    m_del_class(ST7701, self->presto);
    return mp_const_none;
}

}
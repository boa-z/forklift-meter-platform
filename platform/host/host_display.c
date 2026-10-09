#define SDL_MAIN_HANDLED
#include "platform/host/host_platform.h"
#include <SDL.h>
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint32_t pixels[800 * 480];
static lv_display_t *display;
static lv_indev_t *pointer;
static int mouse_x, mouse_y;
static bool mouse_down;
static void flush(lv_display_t *d, const lv_area_t *area, uint8_t *data)
{
    (void)area;
    SDL_UpdateTexture(texture, NULL, data, 800 * 4);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    lv_display_flush_ready(d);
}
static void input(lv_indev_t *i, lv_indev_data_t *data)
{
    (void)i;
    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state = mouse_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
bool meter_host_open(bool hidden)
{
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        return false;
    window =
        SDL_CreateWindow("FIELD - Forklift Reference Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         800, 480, hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN);
    if (!window)
        return false;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer)
        return false;
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 800, 480);
    if (!texture)
        return false;
    display = lv_display_create(800, 480);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display, pixels, NULL, sizeof(pixels), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    pointer = lv_indev_create();
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer, input);
    return true;
}
bool meter_host_events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
            return false;
        if (e.type == SDL_MOUSEMOTION)
        {
            mouse_x = e.motion.x;
            mouse_y = e.motion.y;
        }
        if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP)
        {
            mouse_x = e.button.x;
            mouse_y = e.button.y;
            mouse_down = e.type == SDL_MOUSEBUTTONDOWN;
        }
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
            return false;
    }
    return true;
}
void meter_host_click(int x, int y, bool pressed)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = x;
    e.button.y = y;
    SDL_PushEvent(&e);
}
bool meter_host_capture(const char *path)
{
    lv_refr_now(display);
    SDL_Surface *s =
        SDL_CreateRGBSurfaceWithFormatFrom(pixels, 800, 480, 32, 800 * 4, SDL_PIXELFORMAT_ARGB8888);
    if (!s)
        return false;
    bool ok = SDL_SaveBMP(s, path) == 0;
    SDL_FreeSurface(s);
    return ok;
}
void meter_host_close(void)
{
    if (pointer)
        lv_indev_delete(pointer);
    if (display)
        lv_display_delete(display);
    if (texture)
        SDL_DestroyTexture(texture);
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    SDL_Quit();
}
uint64_t meter_host_counter(void)
{
    return SDL_GetPerformanceCounter();
}
double meter_host_us(uint64_t a, uint64_t b)
{
    return (b - a) * 1000000.0 / SDL_GetPerformanceFrequency();
}
void meter_host_delay(unsigned ms)
{
    SDL_Delay(ms);
}

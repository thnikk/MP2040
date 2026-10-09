//
// MP2040 display simulator.
//
// Compiles the real firmware display sources on the host (see CMakeLists.txt
// and shim/) and renders the actual 1bpp framebuffer to an SDL2 window and/or
// a PNG. Nothing about the menu/layout drawing is reimplemented, so what you
// see is exactly what the panel gets.
//
// Usage:
//   mp2040-display-sim                       # interactive mini menu window
//   mp2040-display-sim --screen buttons      # start on a given screen
//   mp2040-display-sim --screen menu --nav down,down,select --png out.png
//
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <string>
#include <vector>

#include "GPGFX.h"
#include "GPGFX_UI_screens.h"
#include "storagemanager.h"
#include "drivermanager.h"
#include "helper.h"
#include "sim_clock.h"
#include "framebuffer.h"

#ifdef SIM_HAVE_SDL
#include <SDL.h>
#endif

// ------------------------------------------------------------------ helpers

static DisplayMode screenFromName(const std::string& name) {
    if (name == "splash") return SPLASH;
    if (name == "buttons" || name == "layout") return BUTTONS;
    if (name == "menu" || name == "minimenu") return MAIN_MENU;
    if (name == "remap") return REMAP;
    if (name == "saver") return SAVER;
    fprintf(stderr, "unknown screen '%s' (splash|buttons|menu|remap|saver)\n", name.c_str());
    exit(2);
}

static const char* screenName(DisplayMode m) {
    switch (m) {
        case SPLASH: return "splash";
        case BUTTONS: return "buttons";
        case MAIN_MENU: return "menu";
        case REMAP: return "remap";
        case SAVER: return "saver";
    }
    return "?";
}

static GPScreen* makeScreen(DisplayMode m, GPGFX* gfx) {
    switch (m) {
        case SPLASH:   return new SplashScreen(gfx);
        case BUTTONS:  return new ButtonLayoutScreen(gfx);
        case MAIN_MENU:return new MainMenuScreen(gfx);
        case REMAP:    return new RemapScreen(gfx);
        case SAVER:    return new DisplaySaverScreen(gfx);
    }
    return nullptr;
}

// Menu/remap nav action indices (shared by all screens):
// 0=up 1=down 2=left 3=right 4=select 5=back
static int navFromToken(const std::string& t) {
    if (t == "up") return 0;
    if (t == "down") return 1;
    if (t == "left") return 2;
    if (t == "right") return 3;
    if (t == "select" || t == "a" || t == "enter") return 4;
    if (t == "back" || t == "b" || t == "esc") return 5;
    fprintf(stderr, "unknown nav action '%s' (up|down|left|right|select|back)\n", t.c_str());
    exit(2);
}

static std::vector<int> splitNav(const std::string& list) {
    std::vector<int> out;
    size_t start = 0;
    while (start <= list.size()) {
        size_t comma = list.find(',', start);
        std::string tok = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        // trim spaces
        while (!tok.empty() && (tok.front() == ' ' || tok.front() == '\t')) tok.erase(tok.begin());
        while (!tok.empty() && (tok.back() == ' ' || tok.back() == '\t')) tok.pop_back();
        if (!tok.empty()) out.push_back(navFromToken(tok));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return out;
}

static InputMode inputModeFromToken(const std::string& t) {
    if (t == "keyboard" || t == "1") return INPUT_MODE_KEYBOARD;
    if (t == "midi" || t == "2") return INPUT_MODE_MIDI;
    if (t == "xinput" || t == "3") return INPUT_MODE_XINPUT;
    if (t == "switch" || t == "switchpro" || t == "4") return INPUT_MODE_SWITCH_PRO;
    if (t == "xboxone" || t == "5") return INPUT_MODE_XBOX_ONE;
    if (t == "ps3" || t == "6") return INPUT_MODE_PS3;
    if (t == "ps4" || t == "7") return INPUT_MODE_PS4;
    if (t == "ps5" || t == "8") return INPUT_MODE_PS5;
    return (InputMode)atoi(t.c_str());
}

static uint32_t parseColor(const std::string& s, uint32_t dflt) {
    const char* p = s.c_str();
    if (*p == '#') p++;
    char* end = nullptr;
    unsigned long v = strtoul(p, &end, 16);
    if (end == p) return dflt;
    return (uint32_t)v & 0xFFFFFF;
}

// ------------------------------------------------------------------- options

struct Options {
    DisplayMode screen = MAIN_MENU;
    std::string nav;
    std::string png;
    std::string shotDir = "screenshots";
    int scale = 0;              // 0 = auto (8 windowed, 1 headless)
    bool scaleSet = false;
    uint32_t fg = 0xFFFFFF;
    uint32_t bg = 0x000000;
    int timeMs = -1;
    int size = 0;               // GPGFX_DisplaySize override (2 or 3)
    int flip = -1;              // -1 = from board config
    int invert = -1;
    int saverMode = -1;
    std::string inputMode;
    std::string mask;
    bool drawTest = false;
};

static void usage() {
    printf(
        "MP2040 display simulator\n"
        "  --screen <splash|buttons|menu|remap|saver>  screen to show (default menu)\n"
        "  --nav <up,down,left,right,select,back,...>  navigation actions to replay\n"
        "  --png <path>          render one frame to a PNG and exit (no window)\n"
        "  --scale <n>           integer scale (default 8 windowed, 1 for --png)\n"
        "  --fg <RRGGBB>         lit pixel color (default FFFFFF)\n"
        "  --bg <RRGGBB>         unlit pixel color (default 000000)\n"
        "  --flip / --invert     force the panel remap options on\n"
        "  --size <128x64|128x32> override panel size\n"
        "  --saver-mode <n>      screen saver scene (0..5)\n"
        "  --input-mode <n|name> input mode shown in the status bar\n"
        "  --time-ms <n>         virtual clock for animated screens\n"
        "  --shot-dir <dir>      interactive screenshot directory (default screenshots)\n"
        "  --mask <path>         write the 128x64 1bpp bitmask (for pixel SVGs)\n"
        "  --help\n");
}

static Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) { fprintf(stderr, "%s needs a value\n", name); exit(2); }
            return argv[++i];
        };
        if (a == "--screen") o.screen = screenFromName(next("--screen"));
        else if (a == "--nav") o.nav = next("--nav");
        else if (a == "--png") o.png = next("--png");
        else if (a == "--shot-dir") o.shotDir = next("--shot-dir");
        else if (a == "--scale") { o.scale = atoi(next("--scale").c_str()); o.scaleSet = true; }
        else if (a == "--fg") o.fg = parseColor(next("--fg"), o.fg);
        else if (a == "--bg") o.bg = parseColor(next("--bg"), o.bg);
        else if (a == "--flip") o.flip = 1;
        else if (a == "--invert") o.invert = 1;
        else if (a == "--time-ms") o.timeMs = atoi(next("--time-ms").c_str());
        else if (a == "--saver-mode") o.saverMode = atoi(next("--saver-mode").c_str());
        else if (a == "--input-mode") o.inputMode = next("--input-mode");
        else if (a == "--mask") o.mask = next("--mask");
        else if (a == "--draw-test") o.drawTest = true;
        else if (a == "--size") {
            std::string s = next("--size");
            if (s == "128x32") o.size = 2;
            else if (s == "128x64") o.size = 3;
            else { fprintf(stderr, "--size expects 128x64 or 128x32\n"); exit(2); }
        }
        else if (a == "--help" || a == "-h") { usage(); exit(0); }
        else { fprintf(stderr, "unknown option '%s'\n", a.c_str()); usage(); exit(2); }
    }
    if (o.scale == 0) o.scale = o.png.empty() ? 8 : 1;
    return o;
}

// --------------------------------------------------------------------- main

int main(int argc, char** argv) {
    Options opt = parseArgs(argc, argv);

    Storage& storage = Storage::getInstance();
    DisplayOptions& disp = storage.getDisplayOptions();

    if (opt.size != 0) { disp.size = (uint32_t)opt.size; disp.has_size = true; }
    if (opt.saverMode >= 0) { disp.displaySaverMode = (DisplaySaverMode)opt.saverMode; disp.has_displaySaverMode = true; }
    if (!opt.inputMode.empty())
        DriverManager::getInstance().setInputMode(inputModeFromToken(opt.inputMode));

    bool flip = opt.flip >= 0 ? (bool)opt.flip : (disp.has_flip && disp.flip != 0);
    bool invert = opt.invert >= 0 ? (bool)opt.invert : (disp.invert != 0);

    if (opt.timeMs >= 0) simSetVirtualMillis((uint32_t)opt.timeMs);
    else simUseRealClock();

    // Initialise the real renderer the same way DisplayController does.
    GPGFX gfx;
    GPGFX_DisplayTypeOptions gfxOptions;
    gfxOptions.displayType = DISPLAY_TYPE_SSD1306;
    gfxOptions.i2c = nullptr;
    gfxOptions.size = disp.size;
    gfxOptions.address = 0x3C;
    gfxOptions.orientation = flip ? 1 : 0;
    gfxOptions.inverted = invert;
    gfxOptions.font = {6, 8, GP_Font_Standard};
    gfx.init(gfxOptions);

    const uint16_t fw = gfx.getDriver()->getMetrics()->width;
    const uint16_t fh = gfx.getDriver()->getMetrics()->height;

    // --draw-test: dump raw primitives so rendering can be verified without a
    // display. Draws text 'A' at cell (0,0), an ellipse outline at (29,23)
    // r=7 and a rectangle, then prints the pixels as ASCII.
    if (opt.drawTest) {
        gfx.clearScreen();
        gfx.drawEllipse(28, 23, 7, 7, 1, 0);
        gfx.drawEllipse(60, 40, 4, 2, 1, 0);
        gfx.drawLine(0, 40, 127, 55, 1, 0);
        gfx.drawRectangle(100, 4, 118, 52, 1, 0);
        printf("direct primitives:\n");
        Frame frame = extractFrame(gfx, fw, fh, false, false);
        for (int y = 0; y < fh && y < 64; y++) {
            std::string row;
            for (int x = 0; x < fw; x++)
                row += frame.px[y * fw + x] ? '#' : '.';
            printf("%02d %s\n", y, row.c_str());
        }
        return 0;
    }

    DisplayMode mode = opt.screen;
    GPScreen* screen = makeScreen(mode, &gfx);
    if (screen == nullptr) { fprintf(stderr, "no screen\n"); return 1; }
    screen->init();

    // Replay a scripted navigation sequence.
    if (!opt.nav.empty()) {
        for (int action : splitNav(opt.nav)) {
            if (action < 0) continue;
            int8_t r = screen->handleNavigation((uint8_t)action);
            if (r >= 0 && r != (int8_t)mode) {
                mode = (DisplayMode)r;
                screen->shutdown();
                delete screen;
                screen = makeScreen(mode, &gfx);
                screen->init();
            }
        }
    }

    // ---- headless: write a frame as PNG and/or a 1bpp bitmask ----
    if (!opt.png.empty() || !opt.mask.empty()) {
        screen->update();
        screen->draw();
        Frame frame = extractFrame(gfx, fw, fh, flip, invert);
        if (!opt.png.empty()) {
            if (!writePng(opt.png, frame, opt.scale, opt.fg, opt.bg)) {
                fprintf(stderr, "failed to write %s\n", opt.png.c_str());
                return 1;
            }
            printf("wrote %s (%s %ux%u, scale %d)\n", opt.png.c_str(),
                   screenName(mode), fw, fh, opt.scale);
        }
        if (!opt.mask.empty()) {
            if (!writeMask(opt.mask, frame)) {
                fprintf(stderr, "failed to write %s\n", opt.mask.c_str());
                return 1;
            }
            printf("wrote %s (%ux%u 1bpp mask)\n", opt.mask.c_str(), fw, fh);
        }
        screen->shutdown();
        delete screen;
        return 0;
    }

#ifdef SIM_HAVE_SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    int scale = opt.scale;
    auto makeWindow = [&](int s) -> SDL_Window* {
        SDL_Window* w = SDL_CreateWindow(
            "MP2040 display simulator",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            fw * s, fh * s, SDL_WINDOW_SHOWN);
        return w;
    };

    SDL_Window* window = makeWindow(scale);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == nullptr) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_RenderSetLogicalSize(renderer, fw, fh);
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                              SDL_TEXTUREACCESS_STREAMING, fw, fh);
    if (texture == nullptr) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    printf("MP2040 display sim - %s %ux%u (scale %dx)\n",
           screenName(mode), fw, fh, scale);
    printf("  arrows=nav  enter=select  esc=back  [ ]=scale  f=flip  i=invert\n");
    printf("  1-5=screen  s=screenshot  q=quit\n");

    int shotCounter = 0;
    bool running = true;
    // Drive a virtual clock from zero so animated screens (splash close-in,
    // savers) play from the start instead of using wall-clock epoch time.
    const auto simStart = std::chrono::steady_clock::now();
    while (running) {
        if (opt.timeMs < 0)
            simSetVirtualMillis((uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - simStart).count());
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { running = false; break; }
            if (ev.type != SDL_KEYDOWN) continue;
            SDL_Keycode k = ev.key.keysym.sym;
            if (k == SDLK_q) { running = false; break; }
            if (k == SDLK_s) {
                Frame frame = extractFrame(gfx, fw, fh, flip, invert);
                char path[512];
                snprintf(path, sizeof(path), "%s/display-%s-%d.png",
                         opt.shotDir.c_str(), screenName(mode), shotCounter++);
                if (writePng(path, frame, scale, opt.fg, opt.bg))
                    printf("saved %s\n", path);
                continue;
            }
            if (k == SDLK_f) { flip = !flip; continue; }
            if (k == SDLK_i) { invert = !invert; continue; }
            if (k == SDLK_TAB) {
                int m = ((int)mode + 1) % 5;
                screen->shutdown(); delete screen;
                mode = (DisplayMode)m; screen = makeScreen(mode, &gfx); screen->init();
                continue;
            }
            if (k == SDLK_1 || k == SDLK_2 || k == SDLK_3 || k == SDLK_4 || k == SDLK_5) {
                screen->shutdown(); delete screen;
                mode = (DisplayMode)(k - SDLK_1);
                screen = makeScreen(mode, &gfx); screen->init();
                continue;
            }
            if (k == SDLK_LEFTBRACKET || k == SDLK_MINUS) {
                if (scale > 1) { scale--; SDL_SetWindowSize(window, fw * scale, fh * scale); }
                continue;
            }
            if (k == SDLK_RIGHTBRACKET || k == SDLK_EQUALS || k == SDLK_PLUS) {
                scale++; SDL_SetWindowSize(window, fw * scale, fh * scale); continue;
            }

            int action = -1;
            if (k == SDLK_UP) action = 0;
            else if (k == SDLK_DOWN) action = 1;
            else if (k == SDLK_LEFT) action = 2;
            else if (k == SDLK_RIGHT) action = 3;
            else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) action = 4;
            else if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE) action = 5;
            if (action >= 0) {
                int8_t r = screen->handleNavigation((uint8_t)action);
                if (r >= 0 && r != (int8_t)mode) {
                    screen->shutdown(); delete screen;
                    mode = (DisplayMode)r; screen = makeScreen(mode, &gfx); screen->init();
                }
            }
        }

        int8_t u = screen->update();
        if (u >= 0 && u != (int8_t)mode) {
            screen->shutdown(); delete screen;
            mode = (DisplayMode)u; screen = makeScreen(mode, &gfx); screen->init();
        }
        screen->draw();

        Frame frame = extractFrame(gfx, fw, fh, flip, invert);
        std::vector<uint32_t> px = toArgbPixels(frame, 1, opt.fg, opt.bg, 255);
        SDL_UpdateTexture(texture, nullptr, px.data(), fw * 4);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
#else
    fprintf(stderr, "built without SDL2: use --png <path> for headless output\n");
    screen->shutdown();
    delete screen;
    return 1;
#endif

    screen->shutdown();
    delete screen;
    return 0;
}

#include <windows.h>
#include <cstdint>
#include <iostream>
#include <cstring>
#include <vector>

using InterceptionContext = void*;
using InterceptionDevice  = int;

struct InterceptionKeyStroke {
    uint16_t code;
    uint16_t state;
    uint32_t information;
};

struct InterceptionStroke { uint8_t raw[32]; };

#define INTERCEPTION_FILTER_KEY_ALL 0xFFFFu

using PredFn    = int(*)(InterceptionDevice);
using fn_create  = InterceptionContext(*)();
using fn_destroy = void(*)(InterceptionContext);
using fn_filter  = void(*)(InterceptionContext, PredFn, unsigned short);
using fn_wait    = InterceptionDevice(*)(InterceptionContext);
using fn_receive = int(*)(InterceptionContext, InterceptionDevice, InterceptionStroke*, unsigned int);
using fn_send    = int(*)(InterceptionContext, InterceptionDevice, const InterceptionStroke*, unsigned int);
using fn_iskbd   = int(*)(InterceptionDevice);

static fn_create  ic_create;
static fn_destroy ic_destroy;
static fn_filter  ic_filter;
static fn_wait    ic_wait;
static fn_receive ic_receive;
static fn_send    ic_send;
static fn_iskbd   ic_iskbd;

static int isKbd(InterceptionDevice d) { return ic_iskbd(d); }

static bool loadLib(HMODULE& h)
{
    h = LoadLibraryA("interception.dll");
    if (!h) { std::cerr << "Cannot load interception.dll\n"; return false; }
#define L(sym, fn) \
    fn = reinterpret_cast<decltype(fn)>(GetProcAddress(h, sym)); \
    if (!fn) { std::cerr << "Missing: " sym "\n"; return false; }
    L("interception_create_context",  ic_create)
    L("interception_destroy_context", ic_destroy)
    L("interception_set_filter",      ic_filter)
    L("interception_wait",            ic_wait)
    L("interception_receive",         ic_receive)
    L("interception_send",            ic_send)
    L("interception_is_keyboard",     ic_iskbd)
#undef L
    return true;
}

static constexpr uint16_t KEY_UP_BIT = 0x01;
static constexpr uint16_t KEY_E0_BIT = 0x02;

namespace SC {
    constexpr uint16_t W     = 0x11;
    constexpr uint16_t A     = 0x1E;
    constexpr uint16_t S     = 0x1F;
    constexpr uint16_t D     = 0x20;
    constexpr uint16_t UP    = 0x48;
    constexpr uint16_t DOWN  = 0x50;
    constexpr uint16_t LEFT  = 0x4B;
    constexpr uint16_t RIGHT = 0x4D;
}

struct Pair {
    uint16_t a, b;
    bool     ext;
    bool     aDown = false;
    bool     bDown = false;
};

static void sendKey(InterceptionContext ctx, InterceptionDevice dev,
                    uint16_t code, bool ext, bool up)
{
    InterceptionKeyStroke ks{};
    ks.code  = code;
    ks.state = static_cast<uint16_t>((up ? KEY_UP_BIT : 0) | (ext ? KEY_E0_BIT : 0));
    InterceptionStroke s{};
    std::memcpy(&s, &ks, sizeof(ks));
    ic_send(ctx, dev, &s, 1);
}

int main(int argc, char** argv)
{
    bool hidden = (argc > 1 && std::string(argv[1]) == "--hidden");
    if (hidden) {
        HWND hwnd = GetConsoleWindow();
        if (hwnd) ShowWindow(hwnd, SW_HIDE);
    }

    HMODULE hLib;
    if (!loadLib(hLib)) return 1;

    InterceptionContext ctx = ic_create();
    if (!ctx) {
        std::cerr << "Context failed. Install Interception driver and reboot.\n";
        return 1;
    }

    ic_filter(ctx, isKbd, INTERCEPTION_FILTER_KEY_ALL);

    std::vector<Pair> pairs = {
        { SC::A,    SC::D,     false },
        { SC::W,    SC::S,     false },
        { SC::LEFT, SC::RIGHT, true  },
        { SC::UP,   SC::DOWN,  true  },
    };

    if (!hidden) std::cout << "Snap Tap active. Ctrl+C to stop.\n";

    InterceptionDevice dev;
    InterceptionStroke stroke;

    while (ic_receive(ctx, dev = ic_wait(ctx), &stroke, 1) > 0)
    {
        if (!ic_iskbd(dev)) {
            ic_send(ctx, dev, &stroke, 1);
            continue;
        }

        InterceptionKeyStroke ks;
        std::memcpy(&ks, &stroke, sizeof(ks));

        const bool ext    = (ks.state & KEY_E0_BIT) != 0;
        const bool isDown = (ks.state & KEY_UP_BIT) == 0;

        Pair* pair   = nullptr;
        bool  isKeyA = false;

        for (auto& p : pairs) {
            if (p.ext != ext) continue;
            if (p.a == ks.code) { pair = &p; isKeyA = true;  break; }
            if (p.b == ks.code) { pair = &p; isKeyA = false; break; }
        }

        if (!pair) {
            ic_send(ctx, dev, &stroke, 1);
            continue;
        }

        bool&    selfDown  = isKeyA ? pair->aDown : pair->bDown;
        bool&    otherDown = isKeyA ? pair->bDown : pair->aDown;
        uint16_t otherCode = isKeyA ? pair->b     : pair->a;

        if (isDown) {
            if (selfDown) continue;
            selfDown = true;
            if (otherDown) sendKey(ctx, dev, otherCode, ext, true);
            ic_send(ctx, dev, &stroke, 1);
        } else {
            selfDown = false;
            ic_send(ctx, dev, &stroke, 1);
            if (otherDown) sendKey(ctx, dev, otherCode, ext, false);
        }
    }

    ic_destroy(ctx);
    FreeLibrary(hLib);
    return 0;
}

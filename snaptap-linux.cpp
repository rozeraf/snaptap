#include <linux/input.h>
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <cstdint>
#include <cstring>
#include <csignal>
#include <iostream>
#include <cstdint>
#include <vector>
#include <string>

static volatile bool running = true;
static void onSignal(int) { running = false; }

// --- device detection ---

static bool testBit(const uint8_t* bits, int n) {
    return (bits[n / 8] >> (n % 8)) & 1;
}

static bool isKeyboard(int fd) {
    uint8_t ev[EV_MAX / 8 + 1]   = {};
    uint8_t key[KEY_MAX / 8 + 1] = {};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev)),           ev)  < 0) return false;
    if (!testBit(ev, EV_KEY))                                    return false;
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key)), key) < 0) return false;
    return testBit(key, KEY_A)    && testBit(key, KEY_D) &&
           testBit(key, KEY_LEFT) && testBit(key, KEY_RIGHT);
}

static std::vector<int> openKeyboards() {
    std::vector<int> fds;
    DIR* dir = opendir("/dev/input");
    if (!dir) { std::cerr << "Cannot open /dev/input\n"; return fds; }

    struct dirent* ent;
    while ((ent = readdir(dir))) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;
        std::string path = "/dev/input/" + std::string(ent->d_name);
        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        if (!isKeyboard(fd)) { close(fd); continue; }

        if (ioctl(fd, EVIOCGRAB, 1) < 0) {
            std::cerr << "Cannot grab " << path
                      << " - add user to 'input' group or run with sudo\n";
            close(fd);
            continue;
        }

        char name[256] = {};
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        std::cout << "Grabbed: " << path << " (" << name << ")\n";
        fds.push_back(fd);
    }
    closedir(dir);
    return fds;
}

// --- virtual device ---

static int createVirtual() {
    int ufd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (ufd < 0) {
        std::cerr << "Cannot open /dev/uinput. Try: sudo modprobe uinput\n";
        return -1;
    }

    ioctl(ufd, UI_SET_EVBIT, EV_KEY);
    ioctl(ufd, UI_SET_EVBIT, EV_SYN);
    ioctl(ufd, UI_SET_EVBIT, EV_REP);

    for (int i = 0; i < KEY_MAX; i++)
        ioctl(ufd, UI_SET_KEYBIT, i);

    struct uinput_setup us = {};
    us.id.bustype = BUS_USB;
    us.id.vendor  = 0x1234;
    us.id.product = 0x5678;
    strcpy(us.name, "SnapTap Virtual Keyboard");

    if (ioctl(ufd, UI_DEV_SETUP, &us) < 0 || ioctl(ufd, UI_DEV_CREATE) < 0) {
        std::cerr << "Cannot create virtual device\n";
        close(ufd);
        return -1;
    }

    return ufd;
}

static void emit(int ufd, uint16_t type, uint16_t code, int32_t val) {
    struct input_event ev = {};
    ev.type  = type;
    ev.code  = code;
    ev.value = val;
    write(ufd, &ev, sizeof(ev));
}

static void emitKey(int ufd, int code, int val) {
    emit(ufd, EV_KEY, code, val);
    emit(ufd, EV_SYN, SYN_REPORT, 0);
}

// --- snap tap ---

struct Pair {
    int  a, b;
    bool aDown = false;
    bool bDown = false;
};

static std::vector<Pair> pairs = {
    { KEY_A,    KEY_D     },
    { KEY_W,    KEY_S     },
    { KEY_LEFT, KEY_RIGHT },
    { KEY_UP,   KEY_DOWN  },
};

// val: 0=up 1=down 2=repeat
static void handleKey(int ufd, int code, int val) {
    Pair* pair   = nullptr;
    bool  isKeyA = false;

    for (auto& p : pairs) {
        if (p.a == code) { pair = &p; isKeyA = true;  break; }
        if (p.b == code) { pair = &p; isKeyA = false; break; }
    }

    if (!pair) {
        emit(ufd, EV_KEY, code, val);
        emit(ufd, EV_SYN, SYN_REPORT, 0);
        return;
    }

    bool& selfDown  = isKeyA ? pair->aDown : pair->bDown;
    bool& otherDown = isKeyA ? pair->bDown : pair->aDown;
    int   otherCode = isKeyA ? pair->b     : pair->a;

    if (val == 1) {
        if (selfDown) {
            // key repeat - pass through only if other is not held
            if (!otherDown) emitKey(ufd, code, 2);
            return;
        }
        selfDown = true;
        if (otherDown) emitKey(ufd, otherCode, 0);
        emitKey(ufd, code, 1);
    } else if (val == 0) {
        selfDown = false;
        emitKey(ufd, code, 0);
        if (otherDown) emitKey(ufd, otherCode, 1);
    } else {
        // repeat from OS
        if (!otherDown) emitKey(ufd, code, 2);
    }
}

int main() {
    signal(SIGINT,  onSignal);
    signal(SIGTERM, onSignal);

    auto kbds = openKeyboards();
    if (kbds.empty()) {
        std::cerr << "No keyboards found or none could be grabbed.\n";
        return 1;
    }

    int ufd = createVirtual();
    if (ufd < 0) {
        for (int fd : kbds) { ioctl(fd, EVIOCGRAB, 0); close(fd); }
        return 1;
    }

    // wait for virtual device to appear in /dev/input
    usleep(100000);

    std::vector<pollfd> pfds;
    for (int fd : kbds) pfds.push_back({ fd, POLLIN, 0 });

    std::cout << "Snap Tap active. Press Ctrl+C to stop.\n";

    while (running) {
        if (poll(pfds.data(), pfds.size(), 100) <= 0) continue;

        for (auto& pfd : pfds) {
            if (!(pfd.revents & POLLIN)) continue;

            struct input_event ev;
            while (read(pfd.fd, &ev, sizeof(ev)) == sizeof(ev)) {
                if      (ev.type == EV_KEY) handleKey(ufd, ev.code, ev.value);
                else if (ev.type == EV_SYN) {} // we emit our own SYN
                else    emit(ufd, ev.type, ev.code, ev.value);
            }
        }
    }

    std::cout << "\nShutting down.\n";
    for (int fd : kbds) { ioctl(fd, EVIOCGRAB, 0); close(fd); }
    ioctl(ufd, UI_DEV_DESTROY);
    close(ufd);

    return 0;
}

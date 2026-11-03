/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
#include "Keyboard.h"

#include <string.h>

namespace kf {
namespace kb {

namespace {

// Character rows per layer: 10, 9 and 7 keys (row 2 sits between Shift and Backspace).
const char* const kRow0[4] = {"qwertyuiop", "QWERTYUIOP", "1234567890", "1234567890"};
const char* const kRow1[4] = {"asdfghjkl", "ASDFGHJKL", "-/:;()$&@", "[]{}#%^*+"};
const char* const kRow2[4] = {"zxcvbnm", "ZXCVBNM", ".,?!'\"_", "=\\|~<>`"};

constexpr int kW = 96;        // standard key width
constexpr int kWide = 144;    // Shift, Backspace, Cancel, layer toggle, edge keys of row 1

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
Rect mk(int x, int y, int w, int h)
{
    Rect r;
    r.x = (int16_t)x;
    r.y = (int16_t)y;
    r.w = (int16_t)w;
    r.h = (int16_t)h;
    return r;
}

int rowY(int row) { return kFieldH + row * kRowH; }

} // namespace

Rect textField() { return mk(0, 0, 960, kFieldH); }
Rect textBox() { return mk(8, 40, 784, 70); }
Rect showHideButton() { return mk(804, 40, 148, 70); }
Rect keyArea() { return mk(0, kFieldH, 960, kRows * kRowH); }

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Index layout: row 0 = 0..9, row 1 = 10..18, row 2 = 19 Shift, 20..26, 27 Backspace,
// row 3 = 28 Cancel, 29 layer toggle, 30 Space, 31 OK. The keys tile the area fully.
Key key(Layer layer, int index)
{
    Key k;
    if (index < 0 || index >= kKeyCount) { return k; }
    k.index = index;
    int li = (int)layer;
    if (index < 10) {
        k.kind = KeyKind::Char;
        k.ch = kRow0[li][index];
        k.r = mk(index * kW, rowY(0), kW, kRowH);
    } else if (index < 19) {
        int i = index - 10;
        k.kind = KeyKind::Char;
        k.ch = kRow1[li][i];
        // 9 keys over 960 px: the two edge keys are wider, so there is no dead strip.
        int x = (i == 0) ? 0 : kWide + (i - 1) * kW;
        int w = (i == 0 || i == 8) ? kWide : kW;
        k.r = mk(x, rowY(1), w, kRowH);
    } else if (index == 19) {
        k.kind = KeyKind::Shift;
        k.r = mk(0, rowY(2), kWide, kRowH);
    } else if (index < 27) {
        int i = index - 20;
        k.kind = KeyKind::Char;
        k.ch = kRow2[li][i];
        k.r = mk(kWide + i * kW, rowY(2), kW, kRowH);
    } else if (index == 27) {
        k.kind = KeyKind::Backspace;
        k.r = mk(960 - kWide, rowY(2), kWide, kRowH);
    } else if (index == 28) {
        k.kind = KeyKind::Cancel;
        k.r = mk(0, rowY(3), kWide, kRowH);
    } else if (index == 29) {
        k.kind = KeyKind::LayerToggle;
        k.r = mk(kWide, rowY(3), kWide, kRowH);
    } else if (index == 30) {
        k.kind = KeyKind::Space;
        k.ch = ' ';
        k.r = mk(2 * kWide, rowY(3), 432, kRowH);
    } else {
        k.kind = KeyKind::Ok;
        k.r = mk(2 * kWide + 432, rowY(3), 960 - 2 * kWide - 432, kRowH);
    }
    return k;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
Key hitTest(Layer layer, int x, int y)
{
    if (showHideButton().contains(x, y)) {
        Key k;
        k.kind = KeyKind::ShowHide;
        k.r = showHideButton();
        return k;
    }
    if (!keyArea().contains(x, y)) { return Key(); }
    for (int i = 0; i < kKeyCount; ++i) {
        Key k = key(layer, i);
        if (k.r.contains(x, y)) { return k; }
    }
    return Key();
}

int minLength(Mode m)
{
    switch (m) {
    case Mode::Ssid: return 1;
    case Mode::Password: return 8;
    default: return 0;
    }
}

int maxLength(Mode m) { return (m == Mode::Ssid) ? 32 : 63; }

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void Keyboard::begin(Mode mode, const char* initial)
{
    mode_ = mode;
    len_ = 0;
    text_[0] = '\0';
    symbols_ = false;
    sym_page2_ = false;
    shift_ = ShiftState::Off;
    shift_t_ = 0;
    shown_ = true;
    if (initial) {
        int n = (int)strlen(initial);
        if (n > maxLength(mode)) { n = maxLength(mode); }
        memcpy(text_, initial, (size_t)n);
        text_[n] = '\0';
        len_ = n;
    }
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
Layer Keyboard::layer() const
{
    if (symbols_) { return sym_page2_ ? Layer::Sym2 : Layer::Sym1; }
    return (shift_ == ShiftState::Off) ? Layer::Lower : Layer::Upper;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
bool Keyboard::okEnabled() const
{
    if (mode_ == Mode::PasswordOrOpen && len_ == 0) { return true; }
    int lo = (mode_ == Mode::PasswordOrOpen) ? 8 : minLength(mode_);
    return len_ >= lo && len_ <= maxLength(mode_);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
bool Keyboard::type(char c)
{
    if (len_ >= maxLength(mode_)) { return false; }
    text_[len_++] = c;
    text_[len_] = '\0';
    return true;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
uint8_t Keyboard::press(const Key& k, uint32_t now)
{
    bool ok_before = okEnabled();
    uint8_t ch = 0;
    switch (k.kind) {
    case KeyKind::Char:
    case KeyKind::Space:
        if (type(k.ch)) { ch |= kText; }
        if (!symbols_ && shift_ == ShiftState::Once && k.kind == KeyKind::Char) {
            shift_ = ShiftState::Off;
            ch |= kKeys;
        }
        break;
    case KeyKind::Backspace:
        if (len_ > 0) {
            text_[--len_] = '\0';
            ch |= kText;
        }
        break;
    case KeyKind::Shift:
        if (symbols_) {
            sym_page2_ = !sym_page2_;
        } else if (shift_ == ShiftState::Off) {
            shift_ = ShiftState::Once;
            shift_t_ = now;
        } else if (shift_ == ShiftState::Once && (uint32_t)(now - shift_t_) <= kCapsWindowMs) {
            shift_ = ShiftState::Caps;
        } else {
            shift_ = ShiftState::Off;
        }
        ch |= kKeys;
        break;
    case KeyKind::LayerToggle:
        symbols_ = !symbols_;
        sym_page2_ = false;
        shift_ = ShiftState::Off;
        ch |= kKeys;
        break;
    case KeyKind::ShowHide:
        shown_ = !shown_;
        ch |= kField;
        break;
    case KeyKind::Cancel:
        return kCancel;
    case KeyKind::Ok:
        return okEnabled() ? kDone : 0;
    default:
        return 0;
    }
    if (okEnabled() != ok_before) { ch |= kOkKey; }
    return ch;
}

} // namespace kb
} // namespace kf

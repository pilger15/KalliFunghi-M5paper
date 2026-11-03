/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// On-screen keyboard: key geometry, hit-testing and the text-entry rules.
// Pure (no Arduino), so the drawing, the touch handling and the tests use the same keys.
//
// Full screen 960x540, landscape QWERTY. Text field on top (0..116), four key rows of
// 106 px below. Layers: lowercase, uppercase, symbols 1 and 2; together all printable
// ASCII (0x20..0x7E), so any WPA password can be typed.
//
// Shift: off -> one-shot -> (second tap inside kCapsWindowMs) Caps Lock -> off.
// A one-shot Shift clears after one character. In the symbol layers the Shift key
// switches between symbol page 1 and 2.
#pragma once

#include <stdint.h>
#include "Layout.h"

namespace kf {
namespace kb {

enum class Layer : uint8_t { Lower, Upper, Sym1, Sym2 };
enum class KeyKind : uint8_t { None, Char, Shift, Backspace, Space, Cancel, LayerToggle, Ok, ShowHide };
enum class ShiftState : uint8_t { Off, Once, Caps };
// What the text is for; sets the length limits.
enum class Mode : uint8_t {
    Ssid,            // 1..32 bytes
    Password,        // 8..63 (WPA2)
    PasswordOrOpen,  // 8..63, or empty for an open network (hidden network flow)
};

struct Key {
    KeyKind kind = KeyKind::None;
    char ch = 0;      // for Char
    int index = -1;   // 0..kKeyCount-1, -1 for ShowHide / None
    Rect r;
};

constexpr int kFieldH = 116;
constexpr int kRowH = 106;
constexpr int kRows = 4;
constexpr int kKeyCount = 10 + 9 + 9 + 4;   // per layer, same positions in every layer
constexpr int kMaxText = 64;                // buffer incl. terminator
constexpr uint32_t kCapsWindowMs = 400;

Rect textField();    // whole top area (prompt, text box, counter, Show/Hide)
Rect textBox();      // the box with the text
Rect showHideButton();
Rect keyArea();      // all key rows
Key key(Layer layer, int index);
// Key under (x, y): a key, ShowHide, or None.
Key hitTest(Layer layer, int x, int y);

int minLength(Mode m);
int maxLength(Mode m);

class Keyboard {
public:
    // Bits returned by press(): what has to be redrawn or what happened.
    enum Change : uint8_t {
        kText = 1 << 0,     // text field (text, counter)
        kKeys = 1 << 1,     // key labels (layer or Shift state)
        kOkKey = 1 << 2,    // OK changed between enabled and disabled
        kField = 1 << 3,    // whole field (Show/Hide)
        kDone = 1 << 4,     // OK pressed with a valid text
        kCancel = 1 << 5,
    };

    void begin(Mode mode, const char* initial = "");
    uint8_t press(const Key& k, uint32_t now);

    const char* text() const { return text_; }
    int length() const { return len_; }
    Mode mode() const { return mode_; }
    Layer layer() const;
    ShiftState shift() const { return shift_; }
    bool shown() const { return shown_; }
    bool okEnabled() const;

private:
    bool type(char c);

    Mode mode_ = Mode::Password;
    char text_[kMaxText] = "";
    int len_ = 0;
    bool symbols_ = false;
    bool sym_page2_ = false;
    ShiftState shift_ = ShiftState::Off;
    uint32_t shift_t_ = 0;
    bool shown_ = true;
};

} // namespace kb
} // namespace kf

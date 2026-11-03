/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// On-screen keyboard drawing. Black and white only, so DU4 / DU partials look like GC16.
// Each part draws only inside its own rectangle: the App refreshes just that region.
#include "Ui.h"

#include <M5Unified.h>
#include <stdio.h>
#include <string.h>

namespace kf {

namespace {

constexpr uint32_t kBlack = TFT_BLACK;
constexpr uint32_t kWhite = TFT_WHITE;
constexpr int kInset = 4;

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void text(const char* s, int x, int y, const lgfx::IFont* font, textdatum_t datum = textdatum_t::top_left)
{
    auto& d = M5.Display;
    d.setFont(font);
    d.setTextDatum(datum);
    d.drawString(s, x, y);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
const char* specialLabel(const kb::Keyboard& k, kb::KeyKind kind)
{
    bool sym = k.layer() == kb::Layer::Sym1 || k.layer() == kb::Layer::Sym2;
    switch (kind) {
    case kb::KeyKind::Shift:
        if (sym) { return k.layer() == kb::Layer::Sym1 ? "#+=" : "123"; }
        return k.shift() == kb::ShiftState::Caps ? "CAPS" : "Shift";
    case kb::KeyKind::Backspace: return "Del";
    case kb::KeyKind::Space: return "space";
    case kb::KeyKind::Cancel: return "Cancel";
    case kb::KeyKind::LayerToggle: return sym ? "abc" : "123";
    case kb::KeyKind::Ok: return "OK";
    default: return "";
    }
}

} // namespace

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void Ui::drawKbKey(const kb::Keyboard& k, int index, bool pressed)
{
    auto& d = M5.Display;
    kb::Key key = kb::key(k.layer(), index);
    if (key.kind == kb::KeyKind::None) { return; }
    const Rect& r = key.r;
    d.fillRect(r.x, r.y, r.w, r.h, kWhite);
    int x = r.x + kInset;
    int y = r.y + kInset;
    int w = r.w - 2 * kInset;
    int h = r.h - 2 * kInset;

    bool filled = pressed;
    bool enabled = true;
    if (key.kind == kb::KeyKind::Shift && k.layer() == kb::Layer::Upper) { filled = true; }
    if (key.kind == kb::KeyKind::Ok) { enabled = k.okEnabled(); }

    if (filled) {
        d.fillRoundRect(x, y, w, h, 10, kBlack);
        d.setTextColor(kWhite, kBlack);
    } else {
        d.drawRoundRect(x, y, w, h, 10, kBlack);
        if (enabled) { d.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 9, kBlack); }
        d.setTextColor(kBlack, kWhite);
    }
    if (key.kind == kb::KeyKind::Char) {
        char s[2] = {key.ch, '\0'};
        text(s, x + w / 2, y + h / 2, &fonts::FreeSansBold18pt7b, textdatum_t::middle_center);
    } else {
        text(specialLabel(k, key.kind), x + w / 2, y + h / 2,
             enabled ? &fonts::FreeSansBold12pt7b : &fonts::FreeSans12pt7b, textdatum_t::middle_center);
        if (!enabled) {
            // Disabled OK: label crossed out lightly with a dotted line.
            for (int px = x + 20; px < x + w - 20; px += 6) { d.drawPixel(px, y + h / 2 + 18, kBlack); }
        }
    }
    d.setTextColor(kBlack, kWhite);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void Ui::drawKbKeys(const kb::Keyboard& k)
{
    Rect a = kb::keyArea();
    M5.Display.fillRect(a.x, a.y, a.w, a.h, kWhite);
    for (int i = 0; i < kb::kKeyCount; ++i) { drawKbKey(k, i, false); }
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Prompt and counter on top, the text with the cursor at the end (the tail that fits),
// Show/Hide on the right. Hidden text shows one '*' per character.
void Ui::drawKbField(const kb::Keyboard& k, const char* prompt)
{
    auto& d = M5.Display;
    Rect f = kb::textField();
    d.fillRect(f.x, f.y, f.w, f.h, kWhite);
    d.setTextColor(kBlack, kWhite);

    text(prompt, 10, 8, &fonts::FreeSans12pt7b);
    char buf[kb::kMaxText + 4];
    snprintf(buf, sizeof(buf), "%d/%d", k.length(), kb::maxLength(k.mode()));
    text(buf, kb::textBox().x + kb::textBox().w, 8, &fonts::FreeSansBold12pt7b, textdatum_t::top_right);

    Rect b = kb::textBox();
    d.drawRect(b.x, b.y, b.w, b.h, kBlack);
    d.drawRect(b.x + 1, b.y + 1, b.w - 2, b.h - 2, kBlack);

    int n = k.length();
    for (int i = 0; i < n; ++i) { buf[i] = k.shown() ? k.text()[i] : '*'; }
    buf[n] = '\0';
    d.setFont(&fonts::FreeMonoBold18pt7b);
    int cw = d.textWidth("M");
    int fit = (b.w - 40) / (cw > 0 ? cw : 1);
    const char* shown = buf;
    int tx = b.x + 12;
    if (n > fit) {
        // Mark that the start is scrolled out.
        text("<", tx, b.y + b.h / 2, &fonts::FreeMonoBold18pt7b, textdatum_t::middle_left);
        tx += cw;
        shown = buf + (n - fit + 1);
    }
    text(shown, tx, b.y + b.h / 2, &fonts::FreeMonoBold18pt7b, textdatum_t::middle_left);
    int cx = tx + d.textWidth(shown) + 2;
    d.fillRect(cx, b.y + 12, 3, b.h - 24, kBlack);

    drawButton(kb::showHideButton(), k.shown() ? "Hide" : "Show", false);
    d.drawFastHLine(0, f.h - 2, f.w, kBlack);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void Ui::drawKeyboard(const kb::Keyboard& k, const char* prompt)
{
    M5.Display.fillScreen(kWhite);
    drawKbField(k, prompt);
    drawKbKeys(k);
}

} // namespace kf

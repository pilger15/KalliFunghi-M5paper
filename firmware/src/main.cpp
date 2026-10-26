/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Low | Reviewed by: None */
// KalliFunghi firmware entry point. All logic lives in App.

#include <Arduino.h>
#include "App.h"

namespace {
kf::App g_app;
}

/* Edited by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void setup()
{
    g_app.setup();
}

/* Edited by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void loop()
{
    g_app.loop();
}

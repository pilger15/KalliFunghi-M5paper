/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
#include "CsvFormat.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "TimeUtil.h"

namespace kf {

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void formatIsoUtc(int64_t utc, char* buf, size_t len)
{
    Civil c = civilFromUtc(utc);
    snprintf(buf, len, "%04d-%02d-%02dT%02d:%02d:%02dZ", c.year, c.month, c.day, c.hour, c.minute, c.second);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void dayLogPath(int64_t utc, char* buf, size_t len)
{
    Civil c = civilFromUtc(utc);
    snprintf(buf, len, "/log/%04d-%02d-%02d.csv", c.year, c.month, c.day);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
size_t formatRow(const Sample& s, char* buf, size_t len)
{
    char iso[24];
    formatIsoUtc(s.utc, iso, sizeof(iso));
    char scd[40] = ",,";
    if (s.flags & Sample::kScdValid) {
        snprintf(scd, sizeof(scd), "%u,%.2f,%.2f", (unsigned)s.co2, s.temp_c100 / 100.0, s.rh_c100 / 100.0);
    }
    char lux[20] = "";
    if (s.flags & Sample::kLuxValid) {
        snprintf(lux, sizeof(lux), "%.1f", (double)s.lux);
    }
    int n = snprintf(buf, len, "%s,%s,%s,%s,%s", iso, scd, lux,
                     modeName((SamplingMode)s.mode),
                     (s.flags & Sample::kUnsynced) ? "unsynced" : "ntp");
    if (n < 0) { return 0; }
    return ((size_t)n < len) ? (size_t)n : len - 1;
}

namespace {

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
// Copies the next comma-separated field into out; returns pointer after the comma.
const char* nextField(const char* p, char* out, size_t len)
{
    size_t i = 0;
    while (*p && *p != ',' && *p != '\r' && *p != '\n') {
        if (i + 1 < len) { out[i++] = *p; }
        ++p;
    }
    out[i] = '\0';
    return (*p == ',') ? p + 1 : p;
}

} // namespace

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
bool parseRow(const char* line, Sample& out)
{
    char f[7][24];
    const char* p = line;
    for (int i = 0; i < 7; ++i) { p = nextField(p, f[i], sizeof(f[i])); }

    Civil c{};
    if (sscanf(f[0], "%4d-%2d-%2dT%2d:%2d:%2d", &c.year, &c.month, &c.day, &c.hour, &c.minute, &c.second) != 6) {
        return false;
    }
    out = Sample{};
    out.utc = (uint32_t)utcFromCivil(c);
    if (f[1][0] && f[2][0] && f[3][0]) {
        out.co2 = (uint16_t)atoi(f[1]);
        out.temp_c100 = (int16_t)lroundf(strtof(f[2], nullptr) * 100.0f);
        out.rh_c100 = (uint16_t)lroundf(strtof(f[3], nullptr) * 100.0f);
        out.flags |= Sample::kScdValid;
    }
    if (f[4][0]) {
        out.lux = strtof(f[4], nullptr);
        out.flags |= Sample::kLuxValid;
    }
    if (strcmp(f[5], "fast") == 0) { out.mode = (uint8_t)SamplingMode::Fast; }
    else if (strcmp(f[5], "ultra") == 0) { out.mode = (uint8_t)SamplingMode::Ultra; }
    else { out.mode = (uint8_t)SamplingMode::Eco; }
    if (strcmp(f[6], "unsynced") == 0) { out.flags |= Sample::kUnsynced; }
    return true;
}

} // namespace kf

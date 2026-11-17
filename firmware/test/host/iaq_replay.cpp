// Replays a trace recorded with tools/iaq_log.py through the firmware's own air-quality code (not a test).
//
//   iaq_replay trace.csv [k_ah] [ticks_per_ln] > out.csv
//
// Runs the old gas-baseline score (sensor::Tracker) and the VOC index (sensor::VocIndex, with the given humidity
// coefficient and scale) side by side on every valid row, and prints: unix_s, gas_ohm, humidity, old score, voc index,
// label. The SNTP clock of the trace is passed on, so state save/restore decisions behave as on the board.
// Plot or tune offline (k from tools/iaq_fit.py), then put the values into VocConfig.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "sensor/core/airq.hpp"
#include "sensor/core/vocindex.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s trace.csv [k_ah] [ticks_per_ln]\n", argv[0]);
        return 2;
    }
    sensor::VocConfig cfg;
    if (argc > 2) cfg.k_ah = std::strtof(argv[2], nullptr);
    if (argc > 3) cfg.ticks_per_ln = std::strtof(argv[3], nullptr);
    std::FILE* f = std::fopen(argv[1], "r");
    if (!f) {
        std::perror(argv[1]);
        return 1;
    }
    sensor::Tracker old_score;
    sensor::VocIndex voc(cfg);
    std::printf("unix_s,gas_ohm,humidity,old_score,voc_index,label\n");
    char line[512];
    long rows = 0, used = 0;
    while (std::fgets(line, sizeof line, f)) {
        if (std::strncmp(line, "unix_s", 6) == 0) continue;  // header
        ++rows;
        double unix_s = 0, temp = 0, rh = 0, hpa = 0, gas = 0, bme_t = 0;
        int valid = 0, score = 0;
        char level[32] = {}, src[16] = {}, label[256] = {};
        // unix_s,temp_c,humidity,pressure_hpa,gas_ohm,gas_valid,score,level,temp_src,bme_temp_c,label
        const int n = std::sscanf(line, "%lf,%lf,%lf,%lf,%lf,%d,%d,%31[^,],%15[^,],%lf,%255[^\n]", &unix_s, &temp, &rh, &hpa, &gas, &valid, &score, level, src, &bme_t, label);
        if (n < 10) continue;
        sensor::Reading r{static_cast<float>(temp), static_cast<float>(rh), static_cast<float>(hpa), static_cast<float>(gas), valid != 0};
        r.bme_temp_c = static_cast<float>(bme_t);
        r.sht40 = std::strcmp(src, "sht40") == 0;
        const auto q = old_score.update(r);
        const int idx = voc.update(r, static_cast<std::int64_t>(unix_s));
        ++used;
        std::printf("%.1f,%.0f,%.1f,%d,%d,%s\n", unix_s, gas, rh, q.score, idx, n >= 11 ? label : "");
    }
    std::fclose(f);
    std::fprintf(stderr, "%ld rows read, %ld replayed, k=%.4f, S=%.0f, restore=%s\n", rows, used, static_cast<double>(cfg.k_ah),
                 static_cast<double>(cfg.ticks_per_ln), sensor::restore_name(voc.restored()));
    return 0;
}

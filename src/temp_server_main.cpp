#include "agregator.hpp"
#include "line_reader.hpp"
#include "timeutil.hpp"

#include "sqlite_db.hpp"
#include "sqlite_repo.hpp"
#include "http.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>


static void usage() {
    std::cerr <<
      "temp_server:\n"
      "  [--db temp.db]\n"
      "  --source stdin|serial [--port COM11|/dev/ttyUSB0] [--baud 9600]\n"
      "  [--http-host 127.0.0.1] [--http-port 8080]\n"
      "  [--raw-keep-sec 86400] [--hour-keep-sec 2592000] [--compact-sec 300]\n";
}


static bool parse_temp_line(const std::string& line, double& out) {
    std::string s = line;

    s.erase(std::remove(s.begin(), s.end(), '\0'), s.end());

    if (s.rfind("TEMP=", 0) == 0) s = s.substr(5);

    for (char& c : s) if (c == ',') c = '.';

    try {
        out = std::stod(s);
        return true;
    } catch (...) {
        return false;
    }
}


static std::int64_t to_unix(std::chrono::system_clock::time_point tp) {
    return (std::int64_t)std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}


int main(int argc, char** argv) {
    std::string dbPath = "temp.db";

    std::string source = "stdin";
    std::string port;
    int baud = 9600;

    std::string http_host = "127.0.0.1";
    int http_port = 8080;

    long long rawKeepSec  = 24 * 3600;
    long long hourKeepSec = 30 * 24 * 3600;
    long long compactSec  = 300;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* n)->std::string {
            if (i+1 >= argc) { std::cerr << "Missing " << n << "\n"; std::exit(2); }
            return argv[++i];
        };

        if (a == "--db") dbPath = need("--db");
        else if (a == "--source") source = need("--source");
        else if (a == "--port") port = need("--port");
        else if (a == "--baud") baud = std::stoi(need("--baud"));
        else if (a == "--http-host") http_host = need("--http-host");
        else if (a == "--http-port") http_port = std::stoi(need("--http-port"));
        else if (a == "--raw-keep-sec") rawKeepSec = std::stoll(need("--raw-keep-sec"));
        else if (a == "--hour-keep-sec") hourKeepSec = std::stoll(need("--hour-keep-sec"));
        else if (a == "--compact-sec") compactSec = std::stoll(need("--compact-sec"));
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else { std::cerr << "Unknown arg: " << a << "\n"; usage(); return 2; }
    }

    std::unique_ptr<LineReader> reader;
    if (source == "stdin") {
        reader = make_stdin_reader();
    } else if (source == "serial") {
        if (port.empty()) { std::cerr << "Error: --port required for serial\n"; return 2; }
        bool ok = false;
        reader = make_serial_reader(port, baud, ok);
        if (!ok) { std::cerr << "Error: can't open " << port << "\n"; return 2; }
    } else {
        std::cerr << "Error: bad --source\n";
        return 2;
    }

    SqliteDb db(dbPath);
    SqliteRepo repo(db);
    repo.init_schema();

    // HTTP сервер поток
    HttpSimple api(repo);
    std::thread http_thr([&]{
        api.run(http_host, http_port);
    });

    Aggregator hourAgg(timeutil::floor_to_hour);
    Aggregator dayAgg(timeutil::floor_to_day);

    using clock = std::chrono::system_clock;
    auto nextCompact = clock::now() + std::chrono::seconds(compactSec);

    std::cerr << "temp_server started. db=" << dbPath << "\n";

    std::string line;
    while (reader->readLine(line)) {
        double temp = 0;
        if (!parse_temp_line(line, temp)) continue;

        auto now_tp = clock::now();
        auto ts = to_unix(now_tp);

        repo.insert_raw(ts, temp);

        if (auto fin = hourAgg.push(now_tp, temp)) {
            repo.insert_hourly(to_unix(fin->period_start), fin->avg);
        }

        if (auto fin = dayAgg.push(now_tp, temp)) {
            repo.insert_daily(to_unix(fin->period_start), fin->avg);
        }

        auto n = clock::now();
        if (n >= nextCompact) {
            auto now_unix = to_unix(n);

            repo.retention("raw", now_unix - rawKeepSec);
            repo.retention("hourly", now_unix - hourKeepSec);

            auto startYear = timeutil::start_of_current_year(n);
            repo.retention("daily", to_unix(startYear));

            nextCompact = n + std::chrono::seconds(compactSec);
        }
    }

    std::cerr << "temp_server finished\n";
    std::exit(0);
}

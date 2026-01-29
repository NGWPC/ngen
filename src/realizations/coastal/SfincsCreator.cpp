#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "realizations/coastal/SfincsCreator.hpp"
#include "realizations/coastal/SfincsFormulation.hpp"

// ----------------- small POSIX helpers -----------------

static bool path_exists(const std::string& p) {
    struct stat st{};
    return ::stat(p.c_str(), &st) == 0;
}
static bool is_dir(const std::string& p) {
    struct stat st{};
    return (::stat(p.c_str(), &st) == 0) && S_ISDIR(st.st_mode);
}
static bool is_file(const std::string& p) {
    struct stat st{};
    return (::stat(p.c_str(), &st) == 0) && S_ISREG(st.st_mode);
}

// Recursively create a directory like `mkdir -p`
static void mkpath(const std::string& dir) {
    if (dir.empty() || is_dir(dir)) return;

    // Split components
    std::vector<std::string> parts;
    {
        std::string cur;
        for (char c : dir) {
            if (c == '/') {
                if (!cur.empty()) { parts.push_back(cur); cur.clear(); }
            } else cur.push_back(c);
        }
        if (!cur.empty()) parts.push_back(cur);
    }

    std::string acc = (dir[0] == '/') ? "/" : "";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (!acc.empty() && acc.back() != '/') acc.push_back('/');
        acc += parts[i];
        if (is_dir(acc)) continue;

        if (::mkdir(acc.c_str(), 0755) != 0) {
            if (errno == EEXIST && is_dir(acc)) continue;
            throw std::runtime_error(std::string("SfincsCreator: mkdir failed for ")
                                     + acc + ": " + std::strerror(errno));
        }
    }
}

static inline void ensure_dir_exists(const std::string& dir) {
    if (path_exists(dir) && !is_dir(dir)) {
        throw std::runtime_error("SfincsCreator: working_dir exists but is not a directory: " + dir);
    }
    if (!path_exists(dir)) mkpath(dir);
}

static inline void ensure_file_exists(const std::string& path, const char* what) {
    if (!is_file(path)) {
        throw std::runtime_error(std::string("SfincsCreator: missing or invalid ") + what + ": " + path);
    }
}

// ----------------- class methods -----------------

std::unique_ptr<CoastalFormulation>
SfincsCreator::createCoastalFormulation(coastal_config_params const& config,
                                        Simulation_Time const& sim_time) const
{
    auto params = config.params.get_child("params");

    // Required fields
    const std::string model_id     = params.get<std::string>("model_type_name");
    const std::string library_file = params.get<std::string>("library_file");
    const std::string working_dir  = params.get<std::string>("working_dir");

    ensure_file_exists(library_file, "library_file");
    ensure_dir_exists(working_dir);

    // Write init config that SFINCS BMI Initialize() will read
    writeInitConfig(config, sim_time);
    const std::string init_config = working_dir + "/sfincs_config.txt";

    // (Optional) switch cwd for convenience; non-fatal if it fails.
    if (::chdir(working_dir.c_str()) != 0) {
        std::cerr << "SfincsCreator: warning: failed changing cwd to "
                  << working_dir << " (" << std::strerror(errno) << ")\n";
    }

    // TODO: Wire providers here in future (met/offshore/channel).
    return std::make_unique<SfincsFormulation>(
        model_id,
        library_file,
        init_config,
        /* met */ nullptr,
        /* offshore */ nullptr,
        /* channel_flow */ nullptr
    );
}

SfincsCreator* SfincsCreator::clone() const {
    return new SfincsCreator();
}

void SfincsCreator::writeInitConfig(coastal_config_params const& config,
                                    Simulation_Time const& sim_time) const
{
    auto params = config.params.get_child("params");
    const std::string working_dir = params.get<std::string>("working_dir");

    const int model_dt_secs    = params.get<int>("model_time_step_in_secs", 60);
    const int end_time_seconds = params.get<int>("end_time_seconds", 3600);

    const time_t start_time_t = sim_time.get_start_date_time_epoch();
    char buffer[32] = {0};
    {
        struct tm* timeInfo = gmtime(&start_time_t);
        strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", timeInfo);
    }

    const std::string init_config = working_dir + "/sfincs_config.txt";
    std::ofstream ofs(init_config);
    if (!ofs.is_open()) {
        throw std::runtime_error(std::string("SfincsCreator: unable to open init config: ") + init_config);
    }

    ofs << "# SFINCS BMI init file\n";
    ofs << "start_datetime = "   << buffer         << "\n";
    ofs << "dt_seconds = "       << model_dt_secs  << "\n";
    ofs << "end_time_seconds = " << end_time_seconds << "\n";

    // Optional grid hints
    if (params.count("nx") > 0) ofs << "nx = " << params.get<int>("nx") << "\n";
    if (params.count("ny") > 0) ofs << "ny = " << params.get<int>("ny") << "\n";
    if (params.count("dx") > 0) ofs << "dx = " << params.get<double>("dx") << "\n";
    if (params.count("dy") > 0) ofs << "dy = " << params.get<double>("dy") << "\n";
    if (params.count("x0") > 0) ofs << "x0 = " << params.get<double>("x0") << "\n";
    if (params.count("y0") > 0) ofs << "y0 = " << params.get<double>("y0") << "\n";

    ofs.close();
}


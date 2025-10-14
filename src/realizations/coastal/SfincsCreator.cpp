#include <unistd.h>
#include <iostream>
#include <fstream>
#include <cassert>
#include <stdexcept>
#include <string>

#if __has_include(<filesystem>)
  #include <filesystem>
  namespace fs = std::filesystem;
#else
  #include <experimental/filesystem>
  namespace fs = std::experimental::filesystem;
#endif

#include "realizations/coastal/SfincsCreator.h"
#include "realizations/coastal/SfincsFormulation.hpp"

// ----------------- local helpers -----------------

static inline void ensure_dir_exists(const std::string& dir) {
    std::error_code ec;
    if (fs::exists(dir, ec)) {
        if (!fs::is_directory(dir, ec)) {
            throw std::runtime_error("SfincsCreator: working_dir exists but is not a directory: " + dir);
        }
        return;
    }
    if (!fs::create_directories(dir, ec)) {
        throw std::runtime_error("SfincsCreator: failed to create working_dir: " + dir + " (" + ec.message() + ")");
    }
}

static inline void ensure_file_exists(const std::string& path, const char* what) {
    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec)) {
        throw std::runtime_error(std::string("SfincsCreator: missing or invalid ") + what + ": " + path);
    }
}

// ----------------- class methods -----------------

std::unique_ptr<CoastalFormulation>
SfincsCreator::createCoastalFormulation(coastal_config_params const& config,
                                        Simulation_Time const& sim_time) const
{
    // Pull the “params” block exactly like other coastal creators
    auto params = config.params.get_child("params");

    // Required
    const std::string model_id     = params.get<std::string>("model_type_name");
    const std::string library_file = params.get<std::string>("library_file");
    const std::string working_dir  = params.get<std::string>("working_dir");

    ensure_file_exists(library_file, "library_file");
    ensure_dir_exists(working_dir);

    // Create/write the init file SFINCS BMI will parse in initialize(config_path)
    writeInitConfig(config, sim_time);
    const std::string init_config = working_dir + "/sfincs_config.txt";

    // Optional: switch CWD to working_dir for convenience (safe to ignore failures)
    if (chdir(working_dir.c_str()) != 0) {
        std::cerr << "SfincsCreator: warning: failed changing cwd to " << working_dir
                  << " (continuing; using absolute paths)\n";
    }

    // If/when you wire providers, build them here (mirroring SchismCreator).
    // For now pass nullptrs.
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

    // Defaults that match the SFINCS BMI expectations you set up
    const int    model_dt_secs    = params.get<int>("model_time_step_in_secs", 60);
    const int    end_time_seconds = params.get<int>("end_time_seconds", 86400);

    // Start time as UTC YYYYMMDDHHMMSS from Simulation_Time
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

    // Minimal, self-describing init file (extend freely as your BMI parser supports)
    ofs << "# SFINCS BMI init file\n";
    ofs << "start_datetime = " << buffer << "\n";      // e.g., 20150101000000 (UTC)
    ofs << "dt_seconds = "     << model_dt_secs << "\n";
    ofs << "end_time_seconds = " << end_time_seconds << "\n";

    // Optional grid/geo hints (only write if present)
    if (params.count("nx")  > 0) ofs << "nx = "  << params.get<int>("nx")  << "\n";
    if (params.count("ny")  > 0) ofs << "ny = "  << params.get<int>("ny")  << "\n";
    if (params.count("dx")  > 0) ofs << "dx = "  << params.get<double>("dx")  << "\n";
    if (params.count("dy")  > 0) ofs << "dy = "  << params.get<double>("dy")  << "\n";
    if (params.count("x0")  > 0) ofs << "x0 = "  << params.get<double>("x0")  << "\n";
    if (params.count("y0")  > 0) ofs << "y0 = "  << params.get<double>("y0")  << "\n";

    ofs.close();
}


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

// Optional mesh forcing providers (kept here for future inputs)
#include "realizations/coastal/MockProvider.h"
#include "forcing/NetCDFMeshPointsDataProvider.hpp"
#include "forcing/MetMeshPolicy.h"
#include "forcing/FlowMeshPolicy.h"

static void ensure_dir_exists(const std::string& dir) {
    if (dir.empty()) return;
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        if (!fs::create_directories(dir, ec) && ec) {
            throw std::runtime_error("Failed to create working_dir: " + dir + " (" + ec.message() + ")");
        }
    } else if (!fs::is_directory(dir, ec)) {
        throw std::runtime_error("working_dir exists but is not a directory: " + dir);
    }
}

static void require_file_exists(const std::string& path, const char* what) {
    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec)) {
        throw std::runtime_error(std::string("Missing or invalid ") + what + ": " + path);
    }
}

std::unique_ptr<CoastalFormulation>
SfincsCreator::createCoastalFormulation(coastal_config_params const& config,
                                        Simulation_Time const& sim_time) const
{
    auto param_tree = config.params.get_child("params");

    const std::string model_id     = param_tree.get<std::string>("model_type_name");
    const std::string library_file = param_tree.get<std::string>("library_file");
    const std::string working_dir  = param_tree.get<std::string>("working_dir");

    // Optional – only needed when you add inputs and want to drive them from NetCDF
    // const std::string met_forcing_file = param_tree.get<std::string>("met_forcing_netcdf_path", "");

    ensure_dir_exists(working_dir);
    require_file_exists(library_file, "library_file");

    // Prepare init file for BMI initialize(config_file)
    writeInitConfig(config, sim_time);
    const std::string init_config = working_dir + "/sfincs_config.txt";

    // Change CWD (kept to match prior behavior; OK to remove if not needed)
    if (chdir(working_dir.c_str()) != 0) {
        std::cerr << "Warning: Failed changing cwd to " << working_dir
                  << " (continuing; absolute paths will be used)\n";
    }

    // If you want to pre-size something for testing without data files:
    // size_t meshsize = 1000;
    // auto mock = std::make_shared<MockProvider>(meshsize);

    // Future: met provider for rain_rate (supported by BMI set_value on "rain_rate"):
    // std::shared_ptr<NetCDFMeshPointsDataProvider<MetMeshPolicy>> met_provider = nullptr;
    // if (!met_forcing_file.empty()) {
    //     met_provider = std::make_shared<NetCDFMeshPointsDataProvider<MetMeshPolicy>>(
    //         met_forcing_file, /*var_name=*/"rain_rate"  // units m s-1 to match BMI
    //     );
    // }

    // Create the formulation. Current example passes nullptr providers.
    return std::make_unique<SfincsFormulation>(
        model_id,
        library_file,
        init_config,
        MPI_COMM_SELF,
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
    auto param_tree = config.params.get_child("params");
    const std::string working_dir = param_tree.get<std::string>("working_dir");

    // Defaults that align with sfincs_bmi2.f90 (dt defaults to 60 s there)
    const int model_dt_secs     = param_tree.get<int>("model_time_step_in_secs", 60);
    const int end_time_seconds  = param_tree.get<int>("end_time_seconds", 86400); // 1 day default in BMI

    // Optional pre-sizing for future Fortran parsing (safe to write now)
    const int nz = param_tree.get<int>("nz", 0);
    const int nu = param_tree.get<int>("nu", 0);
    const int nv = param_tree.get<int>("nv", 0);

    time_t start_time_t = sim_time.get_start_date_time_epoch();

    char buffer[32];
    auto* tinfo = gmtime(&start_time_t);
    strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", tinfo);

    const std::string init_config = working_dir + "/sfincs_config.txt";
    std::ofstream ofs(init_config);
    if (!ofs) {
        throw std::runtime_error(std::string("Unable to open init config: ") + init_config);
    }

    // Trivial text config; Fortran side can parse these later.
    ofs << "# SFINCS BMI init file\n";
    ofs << "start_datetime = " << buffer << "\n";
    ofs << "dt_seconds = " << model_dt_secs << "\n";
    ofs << "end_time_seconds = " << end_time_seconds << "\n";

    // Future-friendly (ignored by current Fortran, but won’t hurt)
    if (nz > 0) ofs << "nz = " << nz << "\n";
    if (nu > 0) ofs << "nu = " << nu << "\n";
    if (nv > 0) ofs << "nv = " << nv << "\n";

    ofs.close();
}


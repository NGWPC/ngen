#include <unistd.h>
#include <iostream>
#include <fstream>
#include <cassert>
#include <sys/stat.h>

#include "realizations/coastal/SfincsCreator.h"
#include "realizations/coastal/SfincsFormulation.hpp"

#include "realizations/coastal/MockProvider.h"
#include "forcing/NetCDFMeshPointsDataProvider.hpp"
#include "forcing/MetMeshPolicy.h"
#include "forcing/FlowMeshPolicy.h"

namespace {
inline bool file_exists(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}
}

std::unique_ptr<CoastalFormulation>
SfincsCreator::createCoastalFormulation( coastal_config_params const& config,
                                         Simulation_Time const&       sim_time ) const
{
    // Pull parameters (same pattern as SchismCreator)
    auto param_tree = config.params.get_child("params");

    std::string model_id        = param_tree.get<std::string>("model_type_name");
    std::string library_file    = param_tree.get<std::string>("library_file");
    std::string working_dir     = param_tree.get<std::string>("working_dir");

    // Optional forcings (kept consistent with SchismCreator’s interface)
    std::string met_forcing_file       = param_tree.get<std::string>("met_forcing_netcdf_path", "");
    std::string offshore_boundary_file = param_tree.get<std::string>("offshore_boundary_netcdf_path", "");
    std::string flow_boundary_file     = param_tree.get<std::string>("streamflow_boundary_netcdf_path", "");

    // SFINCS config path can be supplied; otherwise we’ll create a minimal stub
    std::string init_config = param_tree.get<std::string>("sfincs_config_path",
                              working_dir + "/sfincs_config.txt");

    // Ensure we’re operating in the intended run directory
    if ( chdir( working_dir.c_str() ) != 0 ) {
        std::cout << "Fatal: Failed changing current working dir to " << working_dir << std::endl;
        throw std::runtime_error("Failed chdir to working_dir: " + working_dir);
    }

    // If caller didn’t provide a config, (create) or (refresh) a minimal stub
    // that your BMI initialize() can parse or ignore as needed.
    if (!file_exists(init_config)) {
        this->writeInitConfig(config, sim_time);
    }

    // NOTE: MockProvider is carried over to match SchismCreator’s pattern.
    // If your SfincsFormulation doesn’t use it, you can ignore/remove.
    size_t meshsize = 552697;  // placeholder consistent with SchismCreator example
    auto provider = std::make_shared<MockProvider>(meshsize);

    // Build optional providers (only if files were supplied)
    time_t start_time_t = sim_time.get_start_date_time_epoch();
    time_t stop_time_t  = sim_time.get_end_date_time_epoch();

    std::shared_ptr<data_access::NetCDFMeshPointsDataProvider<data_access::MetMeshPolicy>> netcdf_met_provider;
    std::shared_ptr<data_access::NetCDFMeshPointsDataProvider<data_access::FlowMeshPolicy>> netcdf_streamflow_provider;

    if (!met_forcing_file.empty()) {
        netcdf_met_provider =
            std::make_shared<data_access::NetCDFMeshPointsDataProvider<data_access::MetMeshPolicy>>(
                met_forcing_file,
                std::chrono::system_clock::from_time_t(start_time_t),
                std::chrono::system_clock::from_time_t(stop_time_t));
        // If your SfincsFormulation has a static checker like Schism:
        SfincsFormulation::check_forcing_provider(*netcdf_met_provider);
    }

    if (!flow_boundary_file.empty()) {
        netcdf_streamflow_provider =
            std::make_shared<data_access::NetCDFMeshPointsDataProvider<data_access::FlowMeshPolicy>>(
                flow_boundary_file,
                std::chrono::system_clock::from_time_t(start_time_t),
                std::chrono::system_clock::from_time_t(stop_time_t));
    }

    // Construct the formulation
    // Signature mirrors the SCHISM one for drop-in integration:
    //   (model_id, library_file, init_config, MPI_COMM_SELF, met_provider, mock_provider, flow_provider)
    return std::make_unique<SfincsFormulation>(
                model_id,
                library_file,
                init_config,
                MPI_COMM_SELF,
                netcdf_met_provider,
                provider,
                netcdf_streamflow_provider
           );
}

SfincsCreator* SfincsCreator::clone() const
{
    return new SfincsCreator();
}

void SfincsCreator::writeInitConfig( coastal_config_params const& config,
                                     Simulation_Time const&       sim_time ) const
{
    auto param_tree = config.params.get_child("params");

    std::string working_dir = param_tree.get<std::string>("working_dir");
    std::string init_config = param_tree.get<std::string>("sfincs_config_path",
                                working_dir + "/sfincs_config.txt");

    // Optional hints (only used if we’re creating a stub file)
    int model_start_time = param_tree.get<int>("model_start_time_in_secs", 0);

    time_t start_time_t = sim_time.get_start_date_time_epoch();

    // Compose a minimal, human-readable stub. Your SFINCS BMI initialize()
    // can read/parse or ignore keys it doesn’t use. We avoid imposing a strict
    // SFINCS native format here (e.g., sfincs.inp) to keep this creator general.
    char buffer[100];
    struct tm* timeInfo = gmtime(&start_time_t);
    strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", timeInfo);

    std::fstream cfg(init_config.c_str(), std::ios::out);
    if (!cfg.is_open()) {
        std::cerr << "Error: Unable to open " << init_config << " for writing!" << std::endl;
        throw std::runtime_error(std::string("FATAL: Unable to open file - ") + init_config);
    }

    cfg << "# -----------------------------------------------\n";
    cfg << "# SFINCS BMI init config (generated by SfincsCreator)\n";
    cfg << "# Working dir: " << working_dir << "\n";
    cfg << "# Start (UTC yyyymmddHHMMSS): " << buffer << "\n";
    cfg << "# -----------------------------------------------\n";
    cfg << "working_dir = " << working_dir << "\n";
    cfg << "model_start_time_secs = " << model_start_time << "\n";
    cfg << "start_datetime_utc = " << buffer << "\n";
    cfg << "# Add/override keys as needed by your BMI initialize().\n";
    cfg.close();
}


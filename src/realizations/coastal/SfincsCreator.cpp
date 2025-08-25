#include <unistd.h>
#include <iostream>
#include <fstream>
#include <cassert>

#include "realizations/coastal/SfincsCreator.h"
#include "realizations/coastal/SfincsFormulation.hpp"

// Optional mesh forcing providers (kept here for future inputs)
#include "realizations/coastal/MockProvider.h"
#include "forcing/NetCDFMeshPointsDataProvider.hpp"
#include "forcing/MetMeshPolicy.h"
#include "forcing/FlowMeshPolicy.h"

std::unique_ptr<CoastalFormulation>
SfincsCreator::createCoastalFormulation(coastal_config_params const& config,
                                        Simulation_Time const& sim_time) const
{
    auto param_tree = config.params.get_child("params");

    std::string model_id     = param_tree.get<std::string>("model_type_name");
    std::string library_file = param_tree.get<std::string>("library_file");
    std::string working_dir  = param_tree.get<std::string>("working_dir");

    // Optional – only needed when you add inputs and want to drive them from NetCDF
    // std::string met_forcing_file = param_tree.get<std::string>(
    //     "met_forcing_netcdf_path", "");

    std::string init_config = working_dir + "/sfincs_config.txt";

    if (chdir(working_dir.c_str()) != 0) {
        std::cerr << "Fatal: Failed changing cwd to " << working_dir << std::endl;
        throw std::runtime_error("Failed chdir to working_dir");
    }

    this->writeInitConfig(config, sim_time);

    // If you want to pre-size something for testing without data files:
    // size_t meshsize = 1000;
    // auto mock = std::make_shared<MockProvider>(meshsize);

    // Create the formulation. Current BMI exposes no inputs, so pass nullptr providers.
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
    std::string working_dir = param_tree.get<std::string>("working_dir");

    // Optional defaults; align with your BMI initialize needs
    int model_dt_secs = param_tree.get<int>("model_time_step_in_secs", 60);

    time_t start_time_t = sim_time.get_start_date_time_epoch();

    char buffer[32];
    auto* tinfo = gmtime(&start_time_t);
    strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", tinfo);

    std::string init_config = working_dir + "/sfincs_config.txt";

    std::ofstream ofs(init_config);
    if (!ofs) {
        throw std::runtime_error(std::string("Unable to open init config: ") + init_config);
    }

    // Trivial text config; your Fortran can parse this if/when you wire it up.
    ofs << "# SFINCS BMI init file\n";
    ofs << "start_datetime = " << buffer << "\n";
    ofs << "dt_seconds = " << model_dt_secs << "\n";
    ofs.close();
}


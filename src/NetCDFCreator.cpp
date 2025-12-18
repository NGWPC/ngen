#include <NetCDFCreator.hpp>
#include <Catchment_Formulation.hpp>
#include <Logger.hpp>

using namespace netCDF;


NetCDFCreator::NetCDFCreator(std::shared_ptr<realization::Formulation_Manager> manager, 
    const std::string& output_name,Simulation_Time const& sim_time)
    :catchmentNcFile_(manager->get_output_root() + output_name + ".nc",NcFile::replace)
    ,sim_time_(std::make_shared<Simulation_Time>(sim_time))
{
    timeDim = catchmentNcFile_.addDim("time", NC_UNLIMITED); //unlimited dimension to append data efficiently without redefining the entire file structure
    NcVar timeVar = catchmentNcFile_.addVar("time", NC_INT64, timeDim);
    catchmentsDim = catchmentNcFile_.addDim("catchments", manager->get_size());

    int start_t = sim_time_->get_current_epoch_time();
    timeVar.putVar(&start_t);
    for (int t_step = 0; t_step < sim_time_->get_total_output_times(); t_step++) {
        int next_t = sim_time_->next_timestep_epoch_time();
        timeVar.putVar(&next_t);
    }

    formulations = std::map<std::string, std::shared_ptr<realization::Catchment_Formulation>>(manager->begin(), manager->end());
    for (auto const& catchment : formulations){
        
    }
}

NetCDFCreator::~NetCDFCreator() = default;

#include <netcdf>
#include <Formulation_Manager.hpp>
#include <Simulation_Time.hpp>
#include <Catchment_Formulation.hpp>

using namespace netCDF;

class NetCDFCreator
{
public:
    NetCDFCreator(std::shared_ptr<realization::Formulation_Manager> manager, 
        const std::string& output_name, Simulation_Time const& sim_time);
    NetCDFCreator() = delete;
    ~NetCDFCreator();

private:
    std::shared_ptr<Simulation_Time> sim_time_;
    NcDim timeDim;
    NcDim catchmentsDim;
    std::map<std::string, std::shared_ptr<realization::Catchment_Formulation>> formulations;
};
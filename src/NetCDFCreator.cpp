#include <NGenConfig.h>

#if NGEN_WITH_NETCDF
#include <NetCDFCreator.hpp>
#include <Catchment_Formulation.hpp>
#include <Logger.hpp>

using namespace netCDF;

NetCDFCreator::NetCDFCreator(std::shared_ptr<realization::Formulation_Manager> manager, 
    const std::string& output_name,Simulation_Time const& sim_time)
{

}

NetCDFCreator::~NetCDFCreator() = default;

#endif // NGEN_WITH_NETCDF
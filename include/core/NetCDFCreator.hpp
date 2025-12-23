#if NGEN_WITH_NETCDF

#include <netcdf>
#include <Formulation_Manager.hpp>
#include <Simulation_Time.hpp>
#include <Catchment_Formulation.hpp>

class NetCDFCreator
{
public:
    NetCDFCreator(std::shared_ptr<realization::Formulation_Manager> manager, 
        const std::string& output_name, Simulation_Time const& sim_time);
    NetCDFCreator() = delete;
    ~NetCDFCreator();
};

#endif // NGEN_WITH_NETCDF
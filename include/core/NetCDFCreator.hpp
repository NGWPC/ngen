#ifndef NGEN_NETCDF_CREATOR_HPP
#define NGEN_NETCDF_CREATOR_HPP

#include <NGenConfig.h>

#if NGEN_WITH_NETCDF
#include <vector>
#include <Formulation_Manager.hpp>
#include <Simulation_Time.hpp>
#include <Catchment_Formulation.hpp>

namespace netCDF {
    class NcVar;
    class NcFile;
}
class NetCDFCreator
{
public:
    NetCDFCreator(std::shared_ptr<realization::Formulation_Manager> manager, 
        const std::string& output_name, Simulation_Time const& sim_time);
    NetCDFCreator() = delete;
    ~NetCDFCreator();
    
    void write_simulations_response_from_formulation(size_t time_index, std::map<std::string, std::string> catchment_output_values);

protected:
    void add_output_variable_info_from_formulation(); 

    std::vector<double> string_split(std::string str, char delimiter);

    void close_ncfile();

private:
    std::shared_ptr<netCDF::NcFile> catchmentNcFile;
    std::shared_ptr<realization::Formulation_Manager> manager_;
    std::shared_ptr<Simulation_Time> sim_time_;
    std::vector<std::string> catchments;
    std::vector<netCDF::NcVar> nc_output_variables;
};
#endif // NGEN_WITH_NETCDF
#endif // NGEN_NETCDF_CREATOR_HPP
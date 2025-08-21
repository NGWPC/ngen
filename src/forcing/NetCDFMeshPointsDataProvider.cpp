#include <NGenConfig.h>

#if NGEN_WITH_NETCDF
#include "NetCDFMeshPointsDataProvider.hpp"
#include <UnitsHelper.hpp>

#include <netcdf>

#include <chrono>
#include <sstream>
#include <iostream>
#include <iterator>

#include "forcing/MetMeshPolicy.h"
#include "forcing/FlowMeshPolicy.h"

namespace data_access {

// Out-of-line class definition after forward-declaration so that the
// header doesn't need #include <netcdf> for NcVar to be a complete
// type there
template <typename MeshPolicy>
struct NetCDFMeshPointsDataProvider<MeshPolicy>::metadata_cache_entry {
    netCDF::NcVar ncVar;
    std::string units;
    double scale_factor;
    double offset;
};

template <typename MeshPolicy>
NetCDFMeshPointsDataProvider<MeshPolicy>::NetCDFMeshPointsDataProvider(std::string input_path, time_point_type sim_start, time_point_type sim_end)
    : sim_start_date_time_epoch(sim_start)
    , sim_end_date_time_epoch(sim_end)
{
    nc_file = std::make_shared<netCDF::NcFile>(input_path, netCDF::NcFile::read);

    MeshPolicy::getTimes( *nc_file, sim_start, this->time_vals, this->time_stride );

    this->variable_names = MeshPolicy::getVarNames( *nc_file );

}

template <typename MeshPolicy>
NetCDFMeshPointsDataProvider<MeshPolicy>::~NetCDFMeshPointsDataProvider() = default;

template <typename MeshPolicy>
void NetCDFMeshPointsDataProvider<MeshPolicy>::finalize()
{
    ncvar_cache.clear();

    if (nc_file != nullptr) {
        nc_file->close();
    }
    nc_file = nullptr;
}

template <typename MeshPolicy>
boost::span<const std::string> NetCDFMeshPointsDataProvider<MeshPolicy>::get_available_variable_names() const
{
    return variable_names;
}

template <typename MeshPolicy>
long NetCDFMeshPointsDataProvider<MeshPolicy>::get_data_start_time() const
{
    return std::chrono::system_clock::to_time_t(time_vals[0]);
#if 0
    //return start_time;
    //FIXME: Matching behavior from CsvMeshPointsForcingProvider, but both are probably wrong!
    return sim_start_date_time_epoch.time_since_epoch().count(); // return start_time + sim_to_data_time_offset;
#endif
}

template <typename MeshPolicy>
long NetCDFMeshPointsDataProvider<MeshPolicy>::get_data_stop_time() const
{
    return std::chrono::system_clock::to_time_t(time_vals.back() + time_stride);
#if 0
    //return stop_time;
    //FIXME: Matching behavior from CsvMeshPointsForcingProvider, but both are probably wrong!
    return sim_end_date_time_epoch.time_since_epoch().count(); // return end_time + sim_to_data_time_offset;
#endif
}

template <typename MeshPolicy>
long NetCDFMeshPointsDataProvider<MeshPolicy>::record_duration() const
{
    return std::chrono::duration_cast<std::chrono::seconds>(time_stride).count();
}

template <typename MeshPolicy>
size_t NetCDFMeshPointsDataProvider<MeshPolicy>::get_ts_index_for_time(const time_t &epoch_time_in) const
{
    // time_t in simulation engine's time frame - i.e. seconds, starting at 0
    auto epoch_time = sim_start_date_time_epoch + std::chrono::seconds(epoch_time_in);

    auto start_time = time_vals.front();
    auto stop_time = time_vals.back() + time_stride;

#ifdef DEBUG_NETCDFMESH
    std::cerr << "NetCDFMeshPointsDataProvider::get_ts_index_for_time: epochZ_time_in = "
	    <<epoch_time_in << std::endl;
    std::cerr << "NetCDFMeshPointsDataProvider::get_ts_index_for_time: start_time = "
	    << std::chrono::system_clock::to_time_t(start_time) << std::endl;
    std::cerr << "NetCDFMeshPointsDataProvider::get_ts_index_for_time: stop_time = "
	    << std::chrono::system_clock::to_time_t(stop_time) << std::endl;
#endif //#ifdef DEBUG_NETCDFMESH

    if (start_time <= epoch_time && epoch_time < stop_time)
    {
        auto offset = epoch_time - start_time;
        auto index = offset / time_stride;
        return index;
    }
    else
    {
        std::stringstream ss;
        ss << "The value " << std::chrono::system_clock::to_time_t(epoch_time) << " was not in the range ["
           << std::chrono::system_clock::to_time_t(start_time) << ", "
           << std::chrono::system_clock::to_time_t(stop_time) << ")\n"
           << SOURCE_LOC << "\n";
        ss << "Off by " << std::chrono::system_clock::to_time_t(epoch_time) - std::chrono::system_clock::to_time_t(start_time) << "\n";
        throw std::out_of_range(ss.str().c_str());
    }
}

template <typename MeshPolicy>
void NetCDFMeshPointsDataProvider<MeshPolicy>::get_values(const selection_type& selector, boost::span<data_type> data)
{
    if (!boost::get<AllPoints>(&selector.points)) throw std::runtime_error("Not implemented - only all_points");

    cache_variable(selector.variable_name);

    auto const& metadata = ncvar_cache[selector.variable_name];
    std::string const& source_units = metadata.units;

    size_t time_index = get_ts_index_for_time(std::chrono::system_clock::to_time_t(selector.init_time));

#ifdef DEBUG_NETCDFMESH
    std::cerr << "NetCDFMeshPointsDataProvider::get_values: var = "
	    << selector.variable_name << std::endl;
#endif //#ifdef DEBUG_NETCDFMESH

    MeshPolicy::get_values( *(this->nc_file), selector, data,
	                                      time_index,
					       metadata.ncVar,
	                                       source_units,
		                               metadata.scale_factor,
		                               metadata.offset );

}

template <typename MeshPolicy>
typename NetCDFMeshPointsDataProvider<MeshPolicy>::data_type NetCDFMeshPointsDataProvider<MeshPolicy>::get_value(const selection_type& selector, ReSampleMethod m)
{
    // Check selector only supports exactly one point index
    const auto* pvec = boost::get<std::vector<int>>(&selector.points);
    if (!pvec || pvec->size() != 1) {
        throw std::runtime_error("get_value expects selector.points to contain exactly one index.");
    }
    size_t pt_index = (*pvec)[0];

    // Cache the variable metadata (units, scale/offset, pointer to ncVar)
    cache_variable(selector.variable_name);
    const auto& metadata = ncvar_cache[selector.variable_name];

    // Map the init_time to time_index
    size_t time_index = get_ts_index_for_time(std::chrono::system_clock::to_time_t(selector.init_time));

    double value = MeshPolicy::get_value( *(this->nc_file), selector, m,
		                              pt_index,
	                                      time_index,
					       metadata.ncVar,
	                                       metadata.units,
		                               metadata.scale_factor,
		                               metadata.offset );

    return value;
}

template <typename MeshPolicy>
void NetCDFMeshPointsDataProvider<MeshPolicy>::cache_variable(std::string const& var_name_in)
{
    std::string var_name = MeshPolicy::convertVarName( var_name_in );

    if (ncvar_cache.find(var_name) != ncvar_cache.end()) return;

    auto ncvar = nc_file->getVar(var_name);

    std::string native_units = "N/A";
    try {
       auto units_att = ncvar.getAtt("units");
       units_att.getValues(native_units);
    } catch (...) {
        // Assume it's just not present, and so keeps the default value
    }

    double scale_factor = 1.0;
    try {
        auto scale_var = ncvar.getAtt("scale_factor");
        if (!scale_var.isNull()) {
            scale_var.getValues(&scale_factor);
        }
    } catch (...) {
        // Assume it's just not present, and so keeps the default value
    }

    double offset = 0.0;
    try {
        auto offset_var = ncvar.getAtt("add_offset");
        if (!offset_var.isNull()) {
            offset_var.getValues(&offset);
        }
    } catch (...) {
        // Assume it's just not present, and so keeps the default value
    }

    ncvar_cache[var_name_in] = {ncvar, native_units, scale_factor, offset};
}

//Explicitly instantiate the needed types.
template class NetCDFMeshPointsDataProvider< MetMeshPolicy >; 
template class NetCDFMeshPointsDataProvider< FlowMeshPolicy >; 

}

#endif

#include <NGenConfig.h>

#if NGEN_WITH_NETCDF
#include "NetCDFMeshPointsDataProvider.hpp"
#include <UnitsHelper.hpp>

#include <netcdf>

#include <chrono>
#include <sstream>
#include <iostream>
#include <iterator>

namespace data_access {

// Out-of-line class definition after forward-declaration so that the
// header doesn't need #include <netcdf> for NcVar to be a complete
// type there
struct NetCDFMeshPointsDataProvider::metadata_cache_entry {
    netCDF::NcVar ncVar;
    std::string units;
    double scale_factor;
    double offset;
};

NetCDFMeshPointsDataProvider::NetCDFMeshPointsDataProvider(std::string input_path, time_point_type sim_start, time_point_type sim_end)
    : sim_start_date_time_epoch(sim_start)
    , sim_end_date_time_epoch(sim_end)
{
    nc_file = std::make_shared<netCDF::NcFile>(input_path, netCDF::NcFile::read);

    auto num_times = nc_file->getDim("time").getSize();
    auto time_var = nc_file->getVar("Time");

    if (time_var.getDimCount() != 1) {
        throw std::runtime_error("'Time' variable has dimension other than 1");
    }

    auto time_unit_att = time_var.getAtt("units");
    std::string time_unit_str;

    time_unit_att.getValues(time_unit_str);
    if (time_unit_str != "minutes since 1970-01-01 00:00:00 UTC") {
        throw std::runtime_error("Time units not exactly as expected");
    }

    std::vector<std::chrono::duration<double, std::ratio<60>>> raw_time(num_times);
    time_var.getVar(raw_time.data());

    time_vals.reserve(num_times);
    for (int i = 0; i < num_times; ++i) {
        // Assume that the system clock's epoch matches Unix time.
        // This is guaranteed from C++20 onwards
        time_vals.push_back(time_point_type(std::chrono::duration_cast<time_point_type::duration>(raw_time[i])));
    }

    time_stride = std::chrono::duration_cast<std::chrono::seconds>(time_vals[1] - time_vals[0]);

    // verify the time stride
    for( size_t i = 1; i < time_vals.size() -1; ++i)
    {
        auto tinterval = time_vals[i+1] - time_vals[i];

        if ( tinterval - time_stride > std::chrono::microseconds(1) ||
             time_stride - tinterval > std::chrono::microseconds(1) )
        {
            throw std::runtime_error("Time intervals in forcing file are not constant");
        }
    }

    std::multimap< std::string, netCDF::NcVar > vars = nc_file->getVars();
    std::transform(vars.begin(), vars.end(), 
		    std::back_inserter(variable_names),
          [](const std::pair< std::string, netCDF::NcVar >& pair) { return pair.first; });
#ifdef DEBUG_NETCDFMESH
    std::cerr << "var names: " << std::endl;
    std::copy( variable_names.begin(), variable_names.end(),
          std::ostream_iterator< std::string > ( std::cerr, " \n" ) );
#endif //#ifdef DEBUG_NETCDFMESH
}

NetCDFMeshPointsDataProvider::~NetCDFMeshPointsDataProvider() = default;

void NetCDFMeshPointsDataProvider::finalize()
{
    ncvar_cache.clear();

    if (nc_file != nullptr) {
        nc_file->close();
    }
    nc_file = nullptr;
}

boost::span<const std::string> NetCDFMeshPointsDataProvider::get_available_variable_names() const
{
    return variable_names;
}

long NetCDFMeshPointsDataProvider::get_data_start_time() const
{
    return std::chrono::system_clock::to_time_t(time_vals[0]);
#if 0
    //return start_time;
    //FIXME: Matching behavior from CsvMeshPointsForcingProvider, but both are probably wrong!
    return sim_start_date_time_epoch.time_since_epoch().count(); // return start_time + sim_to_data_time_offset;
#endif
}

long NetCDFMeshPointsDataProvider::get_data_stop_time() const
{
    return std::chrono::system_clock::to_time_t(time_vals.back() + time_stride);
#if 0
    //return stop_time;
    //FIXME: Matching behavior from CsvMeshPointsForcingProvider, but both are probably wrong!
    return sim_end_date_time_epoch.time_since_epoch().count(); // return end_time + sim_to_data_time_offset;
#endif
}

long NetCDFMeshPointsDataProvider::record_duration() const
{
    return std::chrono::duration_cast<std::chrono::seconds>(time_stride).count();
}

size_t NetCDFMeshPointsDataProvider::get_ts_index_for_time(const time_t &epoch_time_in) const
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

void NetCDFMeshPointsDataProvider::get_values(const selection_type& selector, boost::span<data_type> data)
{
    if (!boost::get<AllPoints>(&selector.points)) throw std::runtime_error("Not implemented - only all_points");

    cache_variable(selector.variable_name);

    auto const& metadata = ncvar_cache[selector.variable_name];
    std::string const& source_units = metadata.units;

    size_t time_index = get_ts_index_for_time(std::chrono::system_clock::to_time_t(selector.init_time));

    // XXX: Ignores the point selection in `selector`
    // Possibly assert somewhere (at startup) that dimensions are actually (Time, Index)
    metadata.ncVar.getVar({time_index, 0}, {1, data.size()}, data.data());

    for (auto& value : data) {
        value = value * metadata.scale_factor + metadata.offset;
    }

    // These mass and and volume flux density units are very close to
    // numerically identical for liquid water at atmospheric
    // conditions, and so we currently treat them as interchangeable
    bool RAINRATE_equivalence =
        selector.variable_name == "RAINRATE" &&
        source_units == "mm s^-1" &&
        selector.output_units == "kg m-2 s-1";

    if (!RAINRATE_equivalence) {
        UnitsHelper::convert_values(source_units, data.data(), selector.output_units, data.data(), data.size());
    }
}

NetCDFMeshPointsDataProvider::data_type NetCDFMeshPointsDataProvider::get_value(const selection_type& selector, ReSampleMethod m)
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

    // Assume time, y, x dimensions
    //size_t ny = nc_file->getDim("y").getSize();
    //size_t nx = nc_file->getDim("x").getSize();

    size_t n_elem = nc_file->getDim( "element-id" ).getSize();

    if ( pt_index >= n_elem ) {
        throw std::out_of_range("Point index exceeds available spatial dimension size (y * x).");
    }

   // size_t y_idx = pt_index / nx;
   // size_t x_idx = pt_index % nx;

    // Read raw value from NetCDF variable
    nc_type vartype = metadata.ncVar.getType().getId();
    data_type raw_value = 0.0;

    if (vartype == NC_FLOAT) {
        float tmp;
        metadata.ncVar.getVar({time_index, pt_index}, {1, 1}, &tmp);
        raw_value = static_cast<data_type>(tmp);
    }
    else if (vartype == NC_DOUBLE) {
        double tmp;
        metadata.ncVar.getVar({time_index, pt_index}, {1, 1}, &tmp);
        raw_value = static_cast<data_type>(tmp);
    }
    else if (vartype == NC_INT || vartype == NC_SHORT || vartype == NC_BYTE) {
        int tmp;
        metadata.ncVar.getVar({time_index, pt_index}, {1, 1}, &tmp);
        raw_value = static_cast<data_type>(tmp);
    }
    else {
        throw std::runtime_error("Unsupported NetCDF variable type in get_value()");
    }

    // Check for _FillValue (missing data)
    try {
        if (!metadata.ncVar.getAtt("_FillValue").isNull()) {
            if (vartype == NC_FLOAT) {
                float fv;
                metadata.ncVar.getAtt("_FillValue").getValues(&fv);
                if (static_cast<float>(raw_value) == fv)
                    throw std::runtime_error("Encountered _FillValue (missing data)");
            } else if (vartype == NC_INT || vartype == NC_SHORT || vartype == NC_BYTE) {
                int fv;
                metadata.ncVar.getAtt("_FillValue").getValues(&fv);
                if (static_cast<int>(raw_value) == fv)
                    throw std::runtime_error("Encountered _FillValue (missing data)");
            }
        }
    } catch (...) {
        // Safe to ignore if _FillValue attribute is missing
    }

    // Apply scale and offset
    data_type value = raw_value * metadata.scale_factor + metadata.offset;

    // Handle RAINRATE unit fix if needed
    bool RAINRATE_equivalence =
        selector.variable_name == "RAINRATE" &&
        metadata.units == "mm s^-1" &&
        selector.output_units == "kg m-2 s-1";

    if (!RAINRATE_equivalence) {
        UnitsHelper::convert_values(metadata.units, &value, selector.output_units, &value, 1);
    }

    return value;
}

void NetCDFMeshPointsDataProvider::cache_variable(std::string const& var_name)
{
    if (ncvar_cache.find(var_name) != ncvar_cache.end()) return;

    auto ncvar = nc_file->getVar(var_name);
    //variable_names.push_back(var_name);

    std::string native_units;
    auto units_att = ncvar.getAtt("units");
    units_att.getValues(native_units);

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

    ncvar_cache[var_name] = {ncvar, native_units, scale_factor, offset};
}

}

#endif

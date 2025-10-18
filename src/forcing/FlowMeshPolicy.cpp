#include <iostream>
#include <vector>
#include <algorithm>
#include <netcdf>
#include <UnitsHelper.hpp>
#include "forcing/FlowMeshPolicy.hpp"

void data_access::FlowMeshPolicy::getTimes( netCDF::NcFile const& nc_file,
                             time_point_type const& start_time,
                             std::vector< time_point_type >& times,
                             std::chrono::seconds& stride )
{
#ifdef DEBUG_NETCDFMESH
    std::cerr << "FlowMeshPolicy::getTimes: " << std::endl;
#endif //#ifdef DEBUG_NETCDFMESH
    auto source_num_times = nc_file.getDim("time_vsource").getSize();
#ifdef DEBUG_NETCDFMESH
    std::cerr << "FlowMeshPolicy::getTimes: get time_vsink" << std::endl;
#endif //#ifdef DEBUG_NETCDFMESH
    auto sink_num_times = nc_file.getDim("time_vsink").getSize();
    if ( source_num_times != sink_num_times ) {
        throw std::runtime_error("'time_vsource' and 'time_vsink' have different dimensions!");
    }

#ifdef DEBUG_NETCDFMESH
    std::cerr << "FlowMeshPolicy::getTimes: get time_vsource" << std::endl;
#endif //#ifdef DEBUG_NETCDFMESH
    auto source_time_var = nc_file.getVar("time_vsource");

    if (source_time_var.getDimCount() != 1) {
        throw std::runtime_error("'time_vsource' variable has dimension other than 1");
    }

#ifdef DEBUG_NETCDFMESH
    std::cerr << "FlowMeshPolicy::getTimes: get var vsink" << std::endl;
#endif //#ifdef DEBUG_NETCDFMESH
    auto sink_time_var = nc_file.getVar("time_vsink");

    if (sink_time_var.getDimCount() != 1) {
        throw std::runtime_error("'time_vink' variable has dimension other than 1");
    }

    std::vector<std::chrono::duration<double>> raw_time(source_num_times);

    source_time_var.getVar(raw_time.data());

    times.reserve(source_num_times);
    for (int i = 0; i < source_num_times; ++i) {
        // Assume that the system clock's epoch matches Unix time.
        // This is guaranteed from C++20 onwards
        times.push_back(time_point_type(
                start_time + std::chrono::duration_cast<time_point_type::duration>(raw_time[i])));
    }

    stride = std::chrono::duration_cast<std::chrono::seconds>(times[1] - times[0]);

    // verify the time stride
    for( size_t i = 1; i < times.size() -1; ++i)
    {
        auto tinterval = times[i+1] - times[i];

        if ( tinterval - stride > std::chrono::microseconds(1) ||
             stride - tinterval > std::chrono::microseconds(1) )
        {
            throw std::runtime_error("Time intervals in forcing file are not constant");
        }
    }
#ifdef DEBUG_NETCDFMESH
    std::cerr << "leaving FlowMeshPolicy::getTimes: " << std::endl;
#endif //#ifdef DEBUG_NETCDFMESH
}

std::vector< std::string > data_access::FlowMeshPolicy::getVarNames( netCDF::NcFile const& nc_file )
{
    std::vector< std::string > varnames;
    std::multimap< std::string, netCDF::NcVar > vars = nc_file.getVars();
    std::transform(vars.begin(), vars.end(), 
                    std::back_inserter(varnames),
          [](const std::pair< std::string, netCDF::NcVar >& pair) { 
                 if ( pair.first == "vsource" )
                 {
                     return std::string( "Q_bnd_source" );
                 }
                 if ( pair.first == "vsink" )
                 {
                     return std::string( "Q_bnd_sink" );
                 }
                 return pair.first; });
#ifdef DEBUG_NETCDFMESH
    std::cerr << "var names: " << std::endl;
    std::copy( varnames.begin(), varnames.end(),
          std::ostream_iterator< std::string > ( std::cerr, " \n" ) );
#endif //#ifdef DEBUG_NETCDFMESH
    return varnames;
}

void data_access::FlowMeshPolicy::get_values( netCDF::NcFile const& nc_file,
                                              MeshPointsSelector const& selector,
                                              boost::span<double> data,
                                              size_t const& time_index,
                                              netCDF::NcVar const& ncvar,
                                              std::string const& source_units,
                                              double const& scale_factor,
                                              double const& offset
                                                      )
{
#ifdef DEBUG_NETCDFMESH
    std::cerr << "time_index = " << time_index << std::endl;
#endif //#ifdef DEBUG_NETCDFMESH

    // XXX: Ignores the point selection in `selector`
    // Possibly assert somewhere (at startup) that dimensions are actually (Time, Index)
    //
    ncvar.getVar({time_index, 0}, {1, data.size()}, data.data());

    for (auto& value : data) {
        value = value * scale_factor + offset;
#ifdef DEBUG_NETCDFMESH
    std::cerr << selector. variable_name << ": value = " << value << std::endl;
#endif //#ifdef DEBUG_NETCDFMESH
    }

}

double data_access::FlowMeshPolicy::get_value( netCDF::NcFile const& nc_file,
                              MeshPointsSelector const& selector, data_access::ReSampleMethod m,
                              size_t const& pt_index,
                              size_t const& time_index,
                        netCDF::NcVar const& ncvar,
                        std::string const& source_units,
                        double const& scale_factor,
                        double const& offset
                       )
{
    size_t n_elem = nc_file.getDim( "nsources" ).getSize();

    if ( pt_index >= n_elem && selector.variable_name == "vsource" ) {
        throw std::out_of_range("Point index exceeds available dimension size - nsources");
    }

    n_elem = nc_file.getDim( "nsinks" ).getSize();
    if ( pt_index >= n_elem && selector.variable_name == "vsink" ) {
        throw std::out_of_range("Point index exceeds available dimension size - nsinks");
    }

    // Read raw value from NetCDF variable
    nc_type vartype = ncvar.getType().getId();
    double raw_value = 0.0;

    if (vartype == NC_FLOAT) {
        float tmp;
        ncvar.getVar({time_index, pt_index}, {1, 1}, &tmp);
        raw_value = static_cast<double>(tmp);
    }
    else if (vartype == NC_DOUBLE) {
        double tmp;
        ncvar.getVar({time_index, pt_index}, {1, 1}, &tmp);
        raw_value = static_cast<double>(tmp);
    }
    else if (vartype == NC_INT || vartype == NC_SHORT || vartype == NC_BYTE) {
        int tmp;
        ncvar.getVar({time_index, pt_index}, {1, 1}, &tmp);
        raw_value = static_cast<double>(tmp);
    }
    else {
        throw std::runtime_error("Unsupported NetCDF variable type in get_value()");
    }

    // Check for _FillValue (missing data)
    try {
        if (!ncvar.getAtt("_FillValue").isNull()) {
            if (vartype == NC_FLOAT) {
                float fv;
                ncvar.getAtt("_FillValue").getValues(&fv);
                if (static_cast<float>(raw_value) == fv)
                    throw std::runtime_error("Encountered _FillValue (missing data)");
            } else if (vartype == NC_DOUBLE) {
                double fv; 
                ncvar.getAtt("_FillValue").getValues(&fv);
                if (static_cast<double>(raw_value) == fv)
                    throw std::runtime_error("Encountered _FillValue (missing data).");
            } else if (vartype == NC_INT || vartype == NC_SHORT || vartype == NC_BYTE) {
                int fv;
                ncvar.getAtt("_FillValue").getValues(&fv);
                if (static_cast<int>(raw_value) == fv)
                    throw std::runtime_error("Encountered _FillValue (missing data)");
            }
        } else {
            // No _FillValue attribute — use NetCDF library defaults
            if (vartype == NC_DOUBLE && raw_value == static_cast<double>(NC_FILL_DOUBLE))
                throw std::runtime_error("Encountered default NC_FILL_DOUBLE missing data.");
            if (vartype == NC_FLOAT && raw_value == static_cast<double>(NC_FILL_FLOAT))
                throw std::runtime_error("Encountered default NC_FILL_FLOAT missing data.");
        }
    } catch (...) {
        // Safe to ignore if _FillValue attribute is missing
    }

    // Apply scale and offset
    double value = raw_value * scale_factor + offset;

    return value;
}

std::string data_access::FlowMeshPolicy::convertVarName( std::string const& var_name_in )
{
      if ( var_name_in == std::string( "Q_bnd_source" ) )
      {
         return std::string( "vsource" );
      }
      if ( var_name_in == std::string( "Q_bnd_sink" ) )
      {
         return std::string( "vsink" );
      }
      return std::string();
}

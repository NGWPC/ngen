#include <iostream>
#include <vector>
#include <netcdf>
#include <UnitsHelper.hpp>
#include "forcing/MetMeshPolicy.h"
#include "forcing/NetCDFMeshPointsDataProvider.hpp"

void data_access::MetMeshPolicy::getTimes( netCDF::NcFile const& nc_file,
		             time_point_type const& start_time,
		             std::vector< time_point_type >& times,
                             std::chrono::seconds& stride )
{
    auto num_times = nc_file.getDim("time").getSize();
    auto time_var = nc_file.getVar("Time");

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

    times.reserve(num_times);
    for (int i = 0; i < num_times; ++i) {
        // Assume that the system clock's epoch matches Unix time.
        // This is guaranteed from C++20 onwards
        times.push_back(time_point_type(std::chrono::duration_cast<time_point_type::duration>(raw_time[i])));
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
}

std::vector< std::string > data_access::MetMeshPolicy::getVarNames( netCDF::NcFile const& nc_file )
{
    std::vector< std::string > varnames;
    std::multimap< std::string, netCDF::NcVar > vars = nc_file.getVars();
    std::transform(vars.begin(), vars.end(), 
		    std::back_inserter(varnames),
          [](const std::pair< std::string, netCDF::NcVar >& pair) { return pair.first; });
#ifdef DEBUG_NETCDFMESH
    std::cerr << "var names: " << std::endl;
    std::copy( varnames.begin(), varnames.end(),
          std::ostream_iterator< std::string > ( std::cerr, " \n" ) );
#endif //#ifdef DEBUG_NETCDFMESH
    return varnames;
}

void data_access::MetMeshPolicy::get_values( netCDF::NcFile const& nc_file,
		                              MeshPointsSelector const& selector,
					      boost::span<double> data,
	                                      size_t const& time_index,
					      netCDF::NcVar const& ncvar,
	                                      std::string const& source_units,
					      double const& scale_factor,
					      double const& offset
	                                      	)
{

    // XXX: Ignores the point selection in `selector`
    // Possibly assert somewhere (at startup) that dimensions are actually (Time, Index)
    ncvar.getVar({time_index, 0}, {1, data.size()}, data.data());

    for (auto& value : data) {
        value = value * scale_factor + offset;
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

double data_access::MetMeshPolicy::get_value( netCDF::NcFile const& nc_file,
		              MeshPointsSelector const& selector, data_access::ReSampleMethod m,
			      size_t const& pt_index,
	                      size_t const& time_index,
			netCDF::NcVar const& ncvar,
	                std::string const& source_units,
			double const& scale_factor,
			double const& offset
	       	)
{
    size_t n_elem = nc_file.getDim( "element-id" ).getSize();

    if ( pt_index >= n_elem ) {
        throw std::out_of_range("Point index exceeds available spatial dimension size (y * x).");
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

    // Handle RAINRATE unit fix if needed
    bool RAINRATE_equivalence =
        selector.variable_name == "RAINRATE" &&
        source_units == "mm s^-1" &&
        selector.output_units == "kg m-2 s-1";

    if (!RAINRATE_equivalence) {
        UnitsHelper::convert_values(source_units, &value, selector.output_units, &value, 1);
    }

    return value;
}

std::string data_access::MetMeshPolicy::convertVarName( std::string const& var_name_in )
{
	return var_name_in;
}

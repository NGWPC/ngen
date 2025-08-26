#include <iostream>
#include <vector>
#include <regex>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <netcdf>
#include <UnitsHelper.hpp>
#include "forcing/TidalMeshPolicy.h"
#include "forcing/NetCDFMeshPointsDataProvider.hpp"

void data_access::TidalMeshPolicy::getTimes( netCDF::NcFile const& nc_file,
		             time_point_type const& start_time,
		             std::vector< time_point_type >& times,
                             std::chrono::seconds& stride )
{
    auto num_times = nc_file.getDim("time").getSize();
    auto time_var = nc_file.getVar("time");

    if (time_var.getDimCount() != 1) {
        throw std::runtime_error("'time' variable has dimension other than 1");
    }

    auto time_unit_att = time_var.getAtt("units");
    std::string time_unit_str;

    std::regex time_unit_pattern( "^seconds since "
		    "(19|20)\\d{2}\\-(0[1-9]|1[1,2])\\-(0[1-9]|[12][0-9]|3[01]) "
		    "([01]\\d|2[0-3]):([0-5]\\d):([0-5]\\d)        \\! NCDASE - BASE_DAT$");

    time_unit_att.getValues(time_unit_str);

    if ( ! regex_match(time_unit_str, time_unit_pattern) ) {
        throw std::runtime_error("Time units not exactly as expected");
    }

    time_point_type basetime = stringToTimePoint(time_unit_str, 
		    std::string( "seconds since "
			    "%Y-%m-%d %H:%M:%S        "
			    "! NCDASE - BASE_DAT"  ));

    auto start_time_att = time_var.getAtt("start_time");
    double start_time_att_val;
    start_time_att.getValues( &start_time_att_val );


    std::vector<std::chrono::duration<double>> raw_time(num_times);
    time_var.getVar(raw_time.data());

    times.reserve(num_times);
    for (int i = 0; i < num_times; ++i) {
        // Assume that the system clock's epoch matches Unix time.
        // This is guaranteed from C++20 onwards
        times.push_back( time_point_type( basetime +
		std::chrono::duration_cast<time_point_type::duration>( 
			std::chrono::duration<double>( start_time_att_val ) )
	 	+ std::chrono::duration_cast<time_point_type::duration>(raw_time[i])));
    }

    stride = std::chrono::duration_cast<std::chrono::seconds>(times[1] - times[0]);

    // verify the time stride
    for( size_t i = 1; i < times.size() -1; ++i)
    {
        auto tinterval = times[i+1] - times[i];

        if ( tinterval - stride > std::chrono::microseconds(1) ||
             stride - tinterval > std::chrono::microseconds(1) )
        {
            throw std::runtime_error("Time intervals in offshore boundary file are not constant");
        }
    }
}

std::vector< std::string > data_access::TidalMeshPolicy::getVarNames( netCDF::NcFile const& nc_file )
{
    std::vector< std::string > varnames;
    std::multimap< std::string, netCDF::NcVar > vars = nc_file.getVars();
    std::transform(vars.begin(), vars.end(), 
		    std::back_inserter(varnames),
          [](const std::pair< std::string, netCDF::NcVar >& pair) { 
	         if ( pair.first == "time_series" )
		 {
		     return std::string( "ETA2_bnd" );
		 }
	         return pair.first; });
#ifdef DEBUG_NETCDFMESH
    std::cerr << "var names: " << std::endl;
    std::copy( varnames.begin(), varnames.end(),
          std::ostream_iterator< std::string > ( std::cerr, " \n" ) );
#endif //#ifdef DEBUG_NETCDFMESH
    return varnames;
}

void data_access::TidalMeshPolicy::get_values( netCDF::NcFile const& nc_file,
		                              MeshPointsSelector const& selector,
					      boost::span<double> data,
	                                      size_t const& time_index,
					      netCDF::NcVar const& ncvar,
	                                      std::string const& source_units,
					      double const& scale_factor,
					      double const& offset
	                                      	)
{
    //check dimesnions for nLevels and nComponents
    size_t nLevels = ncvar.getDim( 2 ).getSize();
    if ( nLevels != 1 )
    {
        throw std::runtime_error("'nLevels' dimension has value other than 1");
    }
    size_t nComponents = ncvar.getDim( 3 ).getSize();
    if ( nComponents != 1 )
    {
        throw std::runtime_error("'nComponents' dimension has value other than 1");
    }

    ncvar.getVar({time_index, 0, 0, 0}, {1, data.size(), 1, 1}, data.data());

    for (auto& value : data) {
        value = value * scale_factor + offset;
    }
}

double data_access::TidalMeshPolicy::get_value( netCDF::NcFile const& nc_file,
		              MeshPointsSelector const& selector, data_access::ReSampleMethod m,
			      size_t const& pt_index,
	                      size_t const& time_index,
			netCDF::NcVar const& ncvar,
	                std::string const& source_units,
			double const& scale_factor,
			double const& offset
	       	)
{
    size_t n_elem = nc_file.getDim( "nOpenBndNodes" ).getSize();

    if ( pt_index >= n_elem ) {
        throw std::out_of_range("Point index exceeds available open boundary node dimension.");
    }

    // Read raw value from NetCDF variable
    nc_type vartype = ncvar.getType().getId();
    double raw_value = 0.0;

    if (vartype == NC_FLOAT) {
        float tmp;
        ncvar.getVar({time_index, pt_index, 0, 0}, {1, 1, 1, 1}, &tmp);
        raw_value = static_cast<double>(tmp);
    }
    else if (vartype == NC_DOUBLE) {
        double tmp;
        ncvar.getVar({time_index, pt_index, 0, 0}, {1, 1, 1, 1}, &tmp);
        raw_value = static_cast<double>(tmp);
    }
    else if (vartype == NC_INT || vartype == NC_SHORT || vartype == NC_BYTE) {
        int tmp;
        ncvar.getVar({time_index, pt_index, 0, 0}, {1, 1, 1, 1}, &tmp);
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

std::string data_access::TidalMeshPolicy::convertVarName( std::string const& var_name_in )
{
      if ( var_name_in == std::string( "ETA2_bnd" ) )
      {
	 return std::string( "time_series" );
      }
      return std::string();
}

typename data_access::TidalMeshPolicy::time_point_type 
                              data_access::TidalMeshPolicy::stringToTimePoint(
		std::string const& datetime_str, std::string const& format_str) {
    std::tm t = {};
    std::istringstream ss(datetime_str);
    ss >> std::get_time(&t, format_str.c_str());

    if (ss.fail()) {
        throw std::runtime_error("Failed to parse datetime string.");
    }

    std::time_t tt = std::mktime(&t);
    return std::chrono::system_clock::from_time_t(tt);
}

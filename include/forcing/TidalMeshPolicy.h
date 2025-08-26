#ifndef TIDALMETHPOLICY_H
#define TIDALMETHPOLICY_H

#include <string>
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <exception>
#include <chrono>
#include "GenericDataProvider.hpp"
#include "DataProviderSelectors.hpp"
#include "DataProvider.hpp"

namespace data_access {
    class TidalMeshPolicy
    {//TidalMeshPolicy
        using time_point_type = std::chrono::time_point<std::chrono::system_clock>;
        public:
                static void getTimes( netCDF::NcFile const& nc_file,
		             time_point_type const& start_time,
		             std::vector< time_point_type >& times,
                             std::chrono::seconds& stride );

                static std::vector< std::string > getVarNames( netCDF::NcFile const& nc_file );

                static void get_values( netCDF::NcFile const& nc_file,
		                     MeshPointsSelector const& selector,
					      boost::span<double> data,
			                      size_t const& time_index,
					      netCDF::NcVar const& ncvar,
	                                      std::string const& source_units,
					      double const& scale_factor,
					      double const& offset );

            static  double get_value( netCDF::NcFile const& nc_file,
			               MeshPointsSelector const& selector, 
			                data_access::ReSampleMethod m,
			      size_t const& pt_index,
	                      size_t const& time_index,
			netCDF::NcVar const& ncvar,
	                std::string const& source_units,
			double const& scale_factor,
			double const& offset );

         static std::string convertVarName( std::string const& var_name_in );

        private:
	 static time_point_type stringToTimePoint(std::string const& datetime_str, 
			                         std::string const& format_str);
    };//TidalMeshPolicy
}
#endif //#ifndef TIDALMETHPOLICY_H

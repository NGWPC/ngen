#pragma once

#include <NGenConfig.h>

#if NGEN_WITH_NETCDF

#include "GenericDataProvider.hpp"
#include "DataProviderSelectors.hpp"

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <chrono>
#include <stdexcept>
#include <exception>
#include <algorithm>

namespace netCDF {
    class NcVar;
    class NcFile;
}

namespace data_access
{
    class NetCDFMeshPointsDataProvider : public MeshPointsDataProvider
    {
        public:

        using time_point_type = std::chrono::time_point<std::chrono::system_clock>;

        NetCDFMeshPointsDataProvider(std::string input_path,
                                     time_point_type sim_start,
                                     time_point_type sim_end);

        ~NetCDFMeshPointsDataProvider();

        void finalize() override;

        /** Return the variables that are accessible by this data provider */
        boost::span<const std::string> get_available_variable_names() const override;

        /** Return the first valid time for which data from the requested variable can be requested */
        long get_data_start_time() const override;

        /** Return the last valid time for which data from the requested variable can be requested */
        long get_data_stop_time() const override;

        /** Return the interval in seconds between records in the data */
        long record_duration() const override;

        /**
         * Get the index of the data time step that contains the given point in time.
         * @param epoch_time The point in time, as a seconds-based epoch time.
         * @return The index of the forcing time step that contains the given point in time.
         * @throws std::out_of_range If the given point is not in any time step.
         */
        size_t get_ts_index_for_time(const time_t &epoch_time) const override;

        /**
         * Get the value of a forcing property for a given time and point index, converting units if needed.
         * @param selector Specifies variable, time, and spatial point
         * @param m Resampling strategy (ignored in current implementation)
         */
        data_type get_value(const selection_type& selector, ReSampleMethod m) override;

        /**
         * Get values of a forcing variable for all points at a given time.
         * Assumes the selector is AllPoints.
         * @param selector Input selector
         * @param data Output buffer with size = y * x
         */
        void get_values(const selection_type& selector, boost::span<data_type> data) override;

        /** Not implemented vector-based version */
        std::vector<data_type> get_values(const selection_type& selector, data_access::ReSampleMethod) override
        {
            throw std::runtime_error("Unimplemented");
        }

        private:

        void cache_variable(std::string const& var_name);

        time_point_type sim_start_date_time_epoch;
        time_point_type sim_end_date_time_epoch;

        std::vector<std::string> variable_names;
        std::vector<time_point_type> time_vals;
        std::chrono::seconds time_stride;

        std::shared_ptr<netCDF::NcFile> nc_file;

        struct metadata_cache_entry;
        std::map<std::string, metadata_cache_entry> ncvar_cache;
    };
}

#endif // NGEN_WITH_NETCDF


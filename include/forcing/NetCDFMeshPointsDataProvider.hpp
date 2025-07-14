#pragma once

#include "GenericMeshProvider.hpp"
#include <memory>
#include <map>
#include <string>

#if NGEN_WITH_NETCDF

#include <netcdf>

namespace data_access {

/**
 * Provides spatial forcing data for mesh points from a NetCDF file.
 * Expects time to be the leading dimension, followed by spatial dimensions (e.g., y, x).
 */
class NetCDFMeshPointsDataProvider : public GenericMeshProvider
{
public:
    using data_type = double;

    struct metadata_cache_entry;

    NetCDFMeshPointsDataProvider(std::string input_path,
                                  time_point_type sim_start,
                                  time_point_type sim_end);

    ~NetCDFMeshPointsDataProvider() override;

    void finalize() override;

    boost::span<const std::string> get_available_variable_names() const override;

    long get_data_start_time() const override;

    long get_data_stop_time() const override;

    long record_duration() const override;

    void get_values(const selection_type& selector, boost::span<data_type> data) override;

    data_type get_value(const selection_type& selector, ReSampleMethod m) override;

private:
    void cache_variable(std::string const& var_name);

    size_t get_ts_index_for_time(const time_t &epoch_time_in) const;

    std::shared_ptr<netCDF::NcFile> nc_file;
    std::vector<time_point_type> time_vals;
    std::chrono::seconds time_stride;

    std::vector<std::string> variable_names;

    std::map<std::string, metadata_cache_entry> ncvar_cache;

    time_point_type sim_start_date_time_epoch;
    time_point_type sim_end_date_time_epoch;

    struct metadata_cache_entry {
        netCDF::NcVar ncVar;
        std::string units;
        double scale_factor;
        double offset;
    };
};

}  // namespace data_access

#endif  // NGEN_WITH_NETCDF


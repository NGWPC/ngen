#ifndef NGEN_CSVPERFEATUREFORCING_H
#define NGEN_CSVPERFEATUREFORCING_H
#include "Logger.hpp"

#include <vector>
#include <set>
#include <cmath>
#include <algorithm>
#include <string>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include "CSV_Reader.h"
#include <ctime>
#include <time.h>
#include <memory>
#include "AorcForcing.hpp"
#include "GenericDataProvider.hpp"
#include "DataProviderSelectors.hpp"
#include <exception>
#include <UnitsHelper.hpp>

/**
 * @brief Forcing class providing time-series precipiation forcing data to the model.
 */
class CsvPerFeatureForcingProvider : public data_access::GenericDataProvider
{
    public:

    CsvPerFeatureForcingProvider(forcing_params forcing_config);

    // BEGIN DataProvider interface methods

    /**
     * @brief the inclusive beginning of the period of time over which this instance can provide data for this forcing.
     *
     * @return The inclusive beginning of the period of time over which this instance can provide this data.
     */
    long get_data_start_time() const override;

    /**
     * @brief the exclusive ending of the period of time over which this instance can provide data for this forcing.
     *
     * @return The exclusive ending of the period of time over which this instance can provide this data.
     */
    long get_data_stop_time() const override;

    /**
     * @brief the duration of one record of this forcing source
     *
     * @return The duration of one record of this forcing source
     */
    long record_duration() const override;

    /**
     * Get the index of the forcing time step that contains the given point in time.
     *
     * An @ref std::out_of_range exception should be thrown if the time is not in any time step.
     *
     * @param epoch_time The point in time, as a seconds-based epoch time.
     * @return The index of the forcing time step that contains the given point in time.
     * @throws std::out_of_range If the given point is not in any time step.
     */
    size_t get_ts_index_for_time(const time_t &epoch_time) const override;

    /**
     * Get the value of a forcing property for an arbitrary time period, converting units if needed.
     *
     * An @ref std::out_of_range exception should be thrown if the data for the time period is not available.
     *
     * @param selector Object storing information about the data to be queried
     * @param m methode to resample data if needed
     * @return The value of the forcing property for the described time period, with units converted if needed.
     * @throws std::out_of_range If data for the time period is not available.
     */
    double get_value(const CatchmentAggrDataSelector& selector, data_access::ReSampleMethod m) override;

    virtual std::vector<double> get_values(const CatchmentAggrDataSelector& selector, data_access::ReSampleMethod m) override;

    bool is_param_sum_over_time_step(const std::string& name) const;

    /**
     * Get whether a property's per-time-step values are each an aggregate sum over the entire time step.
     *
     * Certain properties, like rain fall, are aggregated sums over an entire time step.  Others, such as pressure,
     * are not such sums and instead something else like an instantaneous reading or an average value.
     *
     * It may be the case that forcing data is needed for some discretization different than the forcing time step.
     * This aspect must be known in such cases to perform the appropriate value interpolation.
     *
     * @param name The name of the forcing property for which the current value is desired.
     * @return Whether the property's value is an aggregate sum.
     */
    bool is_property_sum_over_time_step(const std::string& name) const override;

    boost::span<const std::string> get_available_variable_names() const override;

    private:

    /**
     * Get the current value of a forcing param identified by its name.
     *
     * @param name The name of the forcing param for which the current value is desired.
     * @param index The index of the desired forcing time step from which to obtain the value.
     * @return The particular param's value at the given forcing time step.
     */
    double get_value_for_param_name(const std::string& name, int index) const;

    /**
     * @brief Read Forcing Data from CSV
     * Reads only data within the specified model start and end date-times.
     * @param file_name Forcing file name
     */
    void read_csv(std::string const& file_name);

    std::vector<std::string> available_forcings;
    std::unordered_map<std::string, std::string> available_forcings_units;

    /// \todo: Look into aggregation of data, relevant libraries, and storing frequency information
    std::unordered_map<std::string, std::vector<double>> forcing_vectors;

    /// \todo: Consider making epoch time the iterator
    std::vector<time_t> time_epoch_vector;     

    std::string forcing_file_name;

    time_t start_date_time_epoch;
    time_t end_date_time_epoch;
    time_t current_date_time_epoch;
};

/// \todo Consider aggregating precipiation data
/// \todo Make CSV forcing a subclass
/// \todo Consider passing grid to class
/// \todo Consider following GDAL API functionality

#endif // NGEN_CSVPERFEATUREFORCING_H

#ifndef MOCK_PROVIDER_HEADER
#define MOCK_PROVIDER_HEADER

#include <iostream>
#include "forcing/NetCDFMeshPointsDataProvider.hpp"

std::map<std::string, double> input_variables_defaults =
    {
        /* Meteorological Forcings */
        // RAINRATE - precipitation
        {"RAINRATE", 0.01},
        // SFCPRS - surface atmospheric pressure
        {"PSFC", 101325.0},
        // SPFH2m - specific humidity at 2m
        {"Q2D", 0.01},
        // TMP2m - temperature at 2m
        {"T2D", 293},
        // UU10m, VV10m - wind velocity components at 10m
        {"U2D", 1.0},
        {"V2D", 1.0},

        /* Input Boundary Conditions */
        // ETA2_bnd - water surface elevation at the boundaries
        {"ETA2_bnd", 30},
        // Q_bnd - flows at boundaries
        {"Q_bnd_source", 0.1},
        // Q_bnd - flows at boundaries
        {"Q_bnd_sink", 0.1},
    };

struct MockProvider : data_access::DataProvider<double, MeshPointsSelector>
{
    std::vector<double> data;

    MockProvider()
        : data(552697, 0.0)
    {}
    ~MockProvider() = default;

    // Implementation of DataProvider
    std::vector<std::string> variables;
    boost::span<const std::string> get_available_variable_names() const override { return variables; }

    long get_data_start_time() const override { return 0; }
    long get_data_stop_time() const override { return 0; }
    long record_duration() const override { return 3600; }
    size_t get_ts_index_for_time(const time_t &epoch_time) const override { return 1; }

    data_type get_value(const selection_type& selector, data_access::ReSampleMethod m) override { return data[0]; }
    std::vector<data_type> get_values(const selection_type& selector, data_access::ReSampleMethod m) override { throw ""; return data; }
    void get_values(const selection_type& selector, boost::span<double> out_data) override
    {
        auto default_value = input_variables_defaults[selector.variable_name];
        for (auto& val : out_data) {
            val = default_value;
        }
    }
};
#endif //#ifndef MOCK_PROVIDER_HEADER

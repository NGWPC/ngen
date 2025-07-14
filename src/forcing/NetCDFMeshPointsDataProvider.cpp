#include "NetCDFMeshPointsDataProvider.hpp"
#include <units/UnitsHelper.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace data_access;

namespace data_access {

NetCDFMeshPointsDataProvider::NetCDFMeshPointsDataProvider(
    std::string const& filepath,
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point stop
) : filepath(filepath), start_time(start), stop_time(stop) {
    nc_file = std::make_unique<NcFile>(filepath, NcFile::read);

    time_var = std::make_unique<NcVar>(nc_file->get_var("time"));
    y_dim = nc_file->get_dim("y")->size();
    x_dim = nc_file->get_dim("x")->size();
}

std::vector<std::string> NetCDFMeshPointsDataProvider::get_available_variable_names() const {
    std::vector<std::string> result;
    int nvars = nc_file->num_vars();
    for (int i = 0; i < nvars; ++i) {
        NcVar* var = nc_file->get_var(i);
        if (var->num_dims() == 3) {
            result.emplace_back(var->name());
        }
    }
    return result;
}

long NetCDFMeshPointsDataProvider::get_data_start_time() const {
    return std::chrono::system_clock::to_time_t(start_time);
}

long NetCDFMeshPointsDataProvider::get_data_stop_time() const {
    return std::chrono::system_clock::to_time_t(stop_time);
}

long NetCDFMeshPointsDataProvider::record_duration() const {
    return 3600; // hourly
}

size_t NetCDFMeshPointsDataProvider::get_ts_index_for_time(const time_t& epoch_time) const {
    return (epoch_time - get_data_start_time()) / record_duration();
}

NetCDFMeshPointsDataProvider::data_type NetCDFMeshPointsDataProvider::get_value(
    const selection_type& selector,
    ReSampleMethod method
) {
    auto time_index = get_ts_index_for_time(std::chrono::system_clock::to_time_t(selector.time));
    auto var_name = selector.variable_name;
    auto index = selector.indices[0];

    NcVar* var = nc_file->get_var(var_name.c_str());
    if (!var) throw std::runtime_error("Variable not found: " + var_name);

    size_t y = index / x_dim;
    size_t x = index % x_dim;

    float val;
    var->set_cur(time_index, y, x);
    var->get(&val, 1, 1, 1);

    // Handle scale and offset
    float scale = 1.0f, offset = 0.0f;
    var->get_att("scale_factor") ? var->get_att("scale_factor")->get(&scale) : void();
    var->get_att("add_offset") ? var->get_att("add_offset")->get(&offset) : void();

    val = val * scale + offset;

    return UnitsHelper::convert(var_name, val, selector.units);
}

std::vector<NetCDFMeshPointsDataProvider::data_type> NetCDFMeshPointsDataProvider::get_values(
    const selection_type& selector,
    ReSampleMethod method
) {
    std::vector<data_type> result(selector.indices.size());
    get_values(selector, boost::span<data_type>(result));
    return result;
}

void NetCDFMeshPointsDataProvider::get_values(
    const selection_type& selector,
    boost::span<data_type> out_data
) {
    auto time_index = get_ts_index_for_time(std::chrono::system_clock::to_time_t(selector.time));
    auto var_name = selector.variable_name;

    NcVar* var = nc_file->get_var(var_name.c_str());
    if (!var) throw std::runtime_error("Variable not found: " + var_name);

    float scale = 1.0f, offset = 0.0f;
    var->get_att("scale_factor") ? var->get_att("scale_factor")->get(&scale) : void();
    var->get_att("add_offset") ? var->get_att("add_offset")->get(&offset) : void();

    for (size_t i = 0; i < selector.indices.size(); ++i) {
        size_t index = selector.indices[i];
        size_t y = index / x_dim;
        size_t x = index % x_dim;

        float val;
        var->set_cur(time_index, y, x);
        var->get(&val, 1, 1, 1);

        val = val * scale + offset;
        out_data[i] = UnitsHelper::convert(var_name, val, selector.units);
    }
}

} // namespace data_access


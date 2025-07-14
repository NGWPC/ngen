#include <NGenConfig.h>

#if NGEN_WITH_NETCDF

#include "NetCDFMeshPointsDataProvider.hpp"
#include <UnitsHelper.hpp>

#include <netcdf>
#include <chrono>
#include <sstream>

namespace data_access {

// Out-of-line class definition after forward-declaration so that the
// header doesn't need #include <netcdf> for NcVar to be a complete type
struct NetCDFMeshPointsDataProvider::metadata_cache_entry {
    netCDF::NcVar ncVar;
    std::string units;
    double scale_factor;
    double offset;
};

NetCDFMeshPointsDataProvider::NetCDFMeshPointsDataProvider(std::string input_path, time_point_type sim_start, time_point_type sim_end)
    : sim_start_date_time_epoch(sim_start), sim_end_date_time_epoch(sim_end)
{
    nc_file = std::make_shared<netCDF::NcFile>(input_path, netCDF::NcFile::read);

    auto num_times = nc_file->getDim("time").getSize();
    auto time_var = nc_file->getVar("time");

    if (time_var.getDimCount() != 1) {
        throw std::runtime_error("'time' variable has dimension other than 1");
    }

    std::string time_unit_str;
    time_var.getAtt("units").getValues(time_unit_str);
    if (time_unit_str != "minutes since 1970-01-01 00:00:00 UTC") {
        throw std::runtime_error("Time units not exactly as expected");
    }

    std::vector<std::chrono::duration<double, std::ratio<60>>> raw_time(num_times);
    time_var.getVar(raw_time.data());

    time_vals.reserve(num_times);
    for (size_t i = 0; i < num_times; ++i) {
        time_vals.push_back(time_point_type(std::chrono::duration_cast<time_point_type::duration>(raw_time[i])));
    }

    time_stride = std::chrono::duration_cast<std::chrono::seconds>(time_vals[1] - time_vals[0]);

    for (size_t i = 1; i < time_vals.size() - 1; ++i) {
        auto tinterval = time_vals[i + 1] - time_vals[i];
        if (std::abs((tinterval - time_stride).count()) > 1) {
            throw std::runtime_error("Time intervals in forcing file are not constant");
        }
    }
}

NetCDFMeshPointsDataProvider::~NetCDFMeshPointsDataProvider() = default;

void NetCDFMeshPointsDataProvider::finalize() {
    ncvar_cache.clear();
    if (nc_file != nullptr) nc_file->close();
    nc_file = nullptr;
}

boost::span<const std::string> NetCDFMeshPointsDataProvider::get_available_variable_names() const {
    return variable_names;
}

long NetCDFMeshPointsDataProvider::get_data_start_time() const {
    return std::chrono::system_clock::to_time_t(time_vals[0]);
}

long NetCDFMeshPointsDataProvider::get_data_stop_time() const {
    return std::chrono::system_clock::to_time_t(time_vals.back() + time_stride);
}

long NetCDFMeshPointsDataProvider::record_duration() const {
    return std::chrono::duration_cast<std::chrono::seconds>(time_stride).count();
}

size_t NetCDFMeshPointsDataProvider::get_ts_index_for_time(const time_t &epoch_time_in) const {
    auto epoch_time = sim_start_date_time_epoch + std::chrono::seconds(epoch_time_in);
    auto start_time = time_vals.front();
    auto stop_time = time_vals.back() + time_stride;

    if (start_time <= epoch_time && epoch_time < stop_time) {
        auto offset = epoch_time - start_time;
        return offset / time_stride;
    } else {
        std::stringstream ss;
        ss << "Epoch time " << epoch_time_in << " is outside of range.";
        throw std::out_of_range(ss.str());
    }
}

void NetCDFMeshPointsDataProvider::get_values(const selection_type& selector, boost::span<data_type> data) {
    if (!boost::get<AllPoints>(&selector.points)) {
        throw std::runtime_error("Only AllPoints selection is supported.");
    }

    cache_variable(selector.variable_name);
    auto const& metadata = ncvar_cache[selector.variable_name];

    size_t time_index = get_ts_index_for_time(std::chrono::system_clock::to_time_t(selector.init_time));
    size_t ny = nc_file->getDim("y").getSize();
    size_t nx = nc_file->getDim("x").getSize();
    size_t total_points = ny * nx;

    if (data.size() != total_points) {
        throw std::runtime_error("Destination buffer size does not match y * x size.");
    }

    std::vector<data_type> raw_data(total_points);
    metadata.ncVar.getVar({time_index, 0, 0}, {1, ny, nx}, raw_data.data());

    for (size_t i = 0; i < total_points; ++i) {
        data[i] = raw_data[i] * metadata.scale_factor + metadata.offset;
    }

    bool RAINRATE_equivalence =
        selector.variable_name == "RAINRATE" &&
        metadata.units == "mm s^-1" &&
        selector.output_units == "kg m-2 s-1";

    if (!RAINRATE_equivalence) {
        UnitsHelper::convert_values(metadata.units, data.data(), selector.output_units, data.data(), data.size());
    }
}

NetCDFMeshPointsDataProvider::data_type NetCDFMeshPointsDataProvider::get_value(const selection_type& selector, ReSampleMethod m) {
    if (!boost::get<PointIndex>(&selector.points)) {
        throw std::runtime_error("get_value only supports PointIndex selector");
    }

    cache_variable(selector.variable_name);
    const auto& metadata = ncvar_cache[selector.variable_name];

    size_t time_index = get_ts_index_for_time(std::chrono::system_clock::to_time_t(selector.init_time));
    size_t ny = nc_file->getDim("y").getSize();
    size_t nx = nc_file->getDim("x").getSize();
    size_t pt_index = boost::get<PointIndex>(selector.points);

    if (pt_index >= ny * nx) {
        throw std::out_of_range("PointIndex exceeds y*x grid size.");
    }

    size_t y_idx = pt_index / nx;
    size_t x_idx = pt_index % nx;

    data_type value;
    metadata.ncVar.getVar({time_index, y_idx, x_idx}, {1, 1, 1}, &value);
    value = value * metadata.scale_factor + metadata.offset;

    bool RAINRATE_equivalence =
        selector.variable_name == "RAINRATE" &&
        metadata.units == "mm s^-1" &&
        selector.output_units == "kg m-2 s-1";

    if (!RAINRATE_equivalence) {
        UnitsHelper::convert_values(metadata.units, &value, selector.output_units, &value, 1);
    }

    return value;
}

void NetCDFMeshPointsDataProvider::cache_variable(std::string const& var_name) {
    if (ncvar_cache.find(var_name) != ncvar_cache.end()) return;

    auto ncvar = nc_file->getVar(var_name);
    variable_names.push_back(var_name);

    std::string native_units;
    ncvar.getAtt("units").getValues(native_units);

    double scale_factor = 1.0, offset = 0.0;
    try { ncvar.getAtt("scale_factor").getValues(&scale_factor); } catch (...) {}
    try { ncvar.getAtt("add_offset").getValues(&offset); } catch (...) {}

    ncvar_cache[var_name] = {ncvar, native_units, scale_factor, offset};
}

} // namespace data_access

#endif


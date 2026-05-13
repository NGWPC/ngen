#if NGEN_WITH_NETCDF
#include "NetCDFVar.hpp"
#include <netcdf.h>
#include <stdexcept>
#include <numeric>
#include <NGenConfig.h>

NetCDFVar::NetCDFVar(const std::string& name, nc_type type, const std::vector<int>& dims, 
    const std::vector<std::string>& dim_names,int varid, int ncid)
    : name_(name), type_(type), dims_(dims), dim_names_(dim_names), varid_(varid), ncid_(ncid)
{}

const std::string& NetCDFVar::get_name() const { return name_; }
nc_type NetCDFVar::get_type() const { return type_; }
const std::vector<int>& NetCDFVar::get_dims() const { return dims_; }
const std::vector<std::string>& NetCDFVar::get_dim_names() const { return dim_names_; }
size_t NetCDFVar::get_dim_count() const { return dims_.size(); }
int NetCDFVar::get_varid() const { return varid_; }

size_t NetCDFVar::get_dim_size(const std::string& dim_name) const {
    for (size_t i = 0; i < dim_names_.size(); ++i) {
        if (dim_names_[i] == dim_name) {
            return dims_[i];
        }
    }
    throw std::runtime_error(
        "Dimension '" + dim_name + "' not found in variable '" + name_ + "'"
    );
}

size_t NetCDFVar::get_dim_size(size_t idx) const {
    if (idx >= dims_.size()) {
        throw std::out_of_range(
            "Dimension index " + std::to_string(idx) + 
            " out of range for variable '" + name_ + "'"
        );
    }
    return dims_[idx];
}
size_t NetCDFVar::get_total_size() const {
    return std::accumulate(dims_.begin(), dims_.end(), size_t{1}, std::multiplies<size_t>());
}

std::vector<std::string> NetCDFVar::get_string_array_values() const {

    // Safety check for zero dimensions
    if (dims_.empty()) {
        throw std::runtime_error("Variable '" + name_ + "' has no dimensions");
    }
    std::vector<std::string> array_items;
    if (type_ == NC_STRING) {
        if (dims_.size() != 1) {
            throw std::runtime_error("NC_STRING variable must be 1D");
        }
        size_t items_count = dims_[0];
        char** raw_strings = nullptr;
        int retval = nc_get_var_string(ncid_, varid_, raw_strings);
        if (retval != NC_NOERR) {
            throw std::runtime_error(nc_strerror(retval));
        }
        array_items.reserve(items_count);
        for (size_t i = 0; i < items_count; ++i) {
            array_items.emplace_back(raw_strings[i] ? raw_strings[i] : "");
        }
        nc_free_string(items_count, raw_strings);
    }
    else if (type_ == NC_CHAR) {
        if (dims_.size() != 2) {
            throw std::runtime_error("NC_CHAR string variable must be 2D");
        }
        size_t num_strings = dims_[0];
        size_t str_len     = dims_[1];
        std::vector<char> buffer(num_strings * str_len);
        int retval = nc_get_var_text(ncid_, varid_, buffer.data());
        if (retval != NC_NOERR) {
            throw std::runtime_error(nc_strerror(retval));
        }
        array_items.reserve(num_strings);
        for (size_t i = 0; i < num_strings; ++i) {
            const char* start = buffer.data() + (i * str_len);
            std::string s(start, str_len);
            size_t end = s.find_last_not_of(" \0"); // trim trailing spaces/nulls
            if (end != std::string::npos) {
                s.erase(end + 1);
            } else {
                s.clear();
            }
            array_items.push_back(s);
        }
    }
    if (array_items.empty()) {
        throw std::runtime_error("Unsupported variable type called for this function.");
    }
    return array_items;
}

std::vector<double> NetCDFVar::get_time_values() const
{
    // Safety checks for dimensions and data type
    if (dims_.empty()) {
        throw std::runtime_error("Variable '" + name_ + "' has no dimensions");
    }
    if (type_ != NC_DOUBLE) {
        throw std::runtime_error("Variable '" + name_ + "' is not NC_DOUBLE");
    }

    size_t time_count = 0;
    if (dims_.size() == 1) {
        time_count = dims_[0]; // (time)

    }
    else if (dims_.size() == 2) {
        time_count = dims_[1]; // (entity, time)
    } else {
        throw std::runtime_error("Unsupported dimensions for time variable '" + name_ + "'");
    }
    if (time_count == 0) {
        return {};
    }

    std::vector<double> time_values(time_count);
    int retval = NC_NOERR;
    if (dims_.size() == 1) {
        retval = nc_get_var_double(ncid_, varid_, time_values.data());
    } else {
        // Read first entity slice:
        // (entity=0, all times)
        std::vector<size_t> start = {0, 0};
        std::vector<size_t> count = {1, time_count};
        retval = nc_get_vara_double(ncid_, varid_, start.data(), count.data(), time_values.data());
    }
    if (retval != NC_NOERR) {
        throw std::runtime_error(nc_strerror(retval));
    }
    return time_values;
}

void NetCDFVar::add_attribute(const std::string& att_name, const std::string& att_value) {
    attributes_str_[att_name] = att_value;
}

void NetCDFVar::add_attribute(const std::string& att_name, int att_value) {
    attributes_int_[att_name] = att_value;
}

void NetCDFVar::add_attribute(const std::string& att_name, double att_value) {
    attributes_double_[att_name] = att_value;
}

// List attributes
std::vector<std::string> NetCDFVar::list_attributes() const {
    std::vector<std::string> names;
    for(const auto& a : attributes_str_) names.push_back(a.first);
    for(const auto& a : attributes_int_) names.push_back(a.first);
    for(const auto& a : attributes_double_) names.push_back(a.first);
    return names;
}

// Access attribute values
std::string NetCDFVar::get_string_attribute(const std::string& att_name) const {
    auto it = attributes_str_.find(att_name);
    if(it != attributes_str_.end()) return it->second;
    throw std::runtime_error("String attribute not found: " + att_name);
}

int NetCDFVar::get_int_attribute(const std::string& att_name) const {
    auto it = attributes_int_.find(att_name);
    if(it != attributes_int_.end()) return it->second;
    throw std::runtime_error("Int attribute not found: " + att_name);
}

double NetCDFVar::get_double_attribute(const std::string& att_name) const {
    auto it = attributes_double_.find(att_name);
    if(it != attributes_double_.end()) return it->second;
    throw std::runtime_error("Double attribute not found: " + att_name);
}

void NetCDFVar::read_slice(const std::vector<size_t>& start, const std::vector<size_t>& count, double* data) const
{
    int retval = NC_NOERR;
    retval = nc_get_vara_double(ncid_, varid_, start.data(), count.data(), data);
    if (retval != NC_NOERR) {
        throw std::runtime_error(std::string("NetCDF read error: ") + nc_strerror(retval));
    }
}
#endif // NGEN_WITH_NETCDF
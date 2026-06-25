#if NGEN_WITH_NETCDF
#include "NetCDFVar.hpp"
#include "Logger.hpp"
#include <stdexcept>
#include <numeric>
#include <NGenConfig.h>

#define NC_CHECK(err, err_context) \
    do { \
        int retval = (err); \
        if (retval != NC_NOERR) { \
            std::string msg = std::string(err_context) \
                            + ". Internal NetCDF Error: " + std::string(nc_strerror(retval)) \
                            + ". Error origin: " + std::string(__func__); \
            LOG(msg, LogLevel::FATAL); \
        } \
    } while (0)

NetCDFVar::NetCDFVar(const std::string& name, nc_type type, const std::vector<int>& dim_ids, 
    const std::vector<std::string>& dim_names,int varid, int ncid)
    : name_(name), type_(type), dim_ids_(dim_ids), dim_names_(dim_names), varid_(varid), ncid_(ncid)
{}

const std::string& NetCDFVar::get_name() const { return name_; }
nc_type NetCDFVar::get_type() const { return type_; }
const std::vector<int>& NetCDFVar::get_dims() const { return dim_ids_; }
const std::vector<std::string>& NetCDFVar::get_dim_names() const { return dim_names_; }
size_t NetCDFVar::get_dim_count() const { return dim_ids_.size(); }
int NetCDFVar::get_varid() const { return varid_; }

size_t NetCDFVar::get_dim_size(const std::string& dim_name) const {
    if(dim_names_.empty()){ return -1; } //scalar variable with no dimensions.
    for (size_t i = 0; i < dim_names_.size(); ++i) {
        if (dim_names_[i] == dim_name) {
            size_t len;
            NC_CHECK(nc_inq_dimlen(ncid_, dim_ids_[i], &len), "Dimension length retrieval failed");
            return len;
        }
    }
    LOG("Dimension '" + dim_name + "' not found in variable '" + name_ + "'", LogLevel::FATAL);
    throw std::runtime_error("Dimension '" + dim_name + "' not found in variable '" + name_ + "'");
}

size_t NetCDFVar::get_dim_size(size_t dim_id) const {
    size_t len;
    NC_CHECK(nc_inq_dimlen(ncid_, dim_id, &len), "Dimension length retrieval failed");
    return len;
}

std::vector<std::string> NetCDFVar::get_string_array_values() const {

    // Safety check for zero dimensions
    if (dim_ids_.empty()) {
        LOG("Variable '" + name_ + "' has no dimensions.", LogLevel::FATAL);
        throw std::runtime_error("Variable '" + name_ + "' has no dimensions");
    }
    std::vector<std::string> array_items;
    if (type_ == NC_STRING) {
        if (dim_ids_.size() != 1) {
            LOG("Retrieving 1D array values: NC_STRING variable must be 1D", LogLevel::FATAL);
            throw std::runtime_error("Retrieving 1D array values: NC_STRING variable must be 1D");
        }
        size_t items_count = get_dim_size(dim_ids_[0]);
        std::vector<char*> raw_strings(items_count, nullptr);
        NC_CHECK(nc_get_var_string(ncid_, varid_, raw_strings.data()), "Retrieving 1D string array values failed");

        array_items.reserve(items_count);
        for (size_t i = 0; i < items_count; ++i) {
            array_items.emplace_back(raw_strings[i] ? raw_strings[i] : "");
        }
        NC_CHECK(nc_free_string(items_count, raw_strings.data()), "Memory free up failed");
    }
    if (array_items.empty()) {
        LOG("Unsupported variable type called for this function.", LogLevel::FATAL);
        throw std::runtime_error("Unsupported variable type called for this function.");
    }
    return array_items;
}

std::vector<std::string> NetCDFVar::get_int64_array_values() const {

    // Safety check for zero dimensions
    if (dim_ids_.empty()) {
        LOG("Variable '" + name_ + "' has no dimensions.", LogLevel::FATAL);
        throw std::runtime_error("Variable '" + name_ + "' has no dimensions");
    }
    std::vector<std::string> array_items;
    if (type_ == NC_STRING) {
        if (dim_ids_.size() != 1) {
            LOG("Retrieving 1D array values: NC_STRING variable must be 1D", LogLevel::FATAL);
            throw std::runtime_error("Retrieving 1D array values: NC_STRING variable must be 1D");
        }
        size_t items_count = get_dim_size(dim_ids_[0]);
        std::vector<char*> raw_strings(items_count, nullptr);
        NC_CHECK(nc_get_var_string(ncid_, varid_, raw_strings.data()), "Retrieving 1D string array values failed");

        array_items.reserve(items_count);
        for (size_t i = 0; i < items_count; ++i) {
            array_items.emplace_back(raw_strings[i] ? raw_strings[i] : "");
        }
        NC_CHECK(nc_free_string(items_count, raw_strings.data()), "Memory free up failed");
    }
    if (array_items.empty()) {
        LOG("Unsupported variable type called for this function.", LogLevel::FATAL);
        throw std::runtime_error("Unsupported variable type called for this function.");
    }
    return array_items;
}

std::vector<double> NetCDFVar::get_time_values() const
{
    // Safety checks for dimensions and data type
    if (dim_ids_.empty()) {
        LOG("Variable '" + name_ + "' has no dimensions.", LogLevel::FATAL);
        throw std::runtime_error("Variable '" + name_ + "' has no dimensions.");
    }
    if (type_ != NC_DOUBLE) {
        LOG("Variable '" + name_ + "' is not NC_DOUBLE.", LogLevel::FATAL);
        throw std::runtime_error("Variable '" + name_ + "' is not NC_DOUBLE.");
    }

    size_t time_count = 0;
    if (dim_ids_.size() == 1) {
        time_count = get_dim_size(dim_ids_[0]); // (time)

    }
    else if (dim_ids_.size() == 2) {
        time_count = get_dim_size(dim_ids_[1]); // (catchment, time)
    } else {
        LOG("Unsupported dimensions for time variable '" + name_ + "'", LogLevel::FATAL);
        throw std::runtime_error("Unsupported dimensions for time variable '" + name_ + "'");
    }
    if (time_count == 0) {
        LOG("Number of timesteps defined for variable '" + name_ + "' is zero.", LogLevel::FATAL);
        throw std::runtime_error("Number of timesteps defined for variable '" + name_ + "' is zero.");
    }

    std::vector<double> time_values(time_count);
    int retval = NC_NOERR;
    if (dim_ids_.size() == 1) {
        NC_CHECK(nc_get_var_double(ncid_, varid_, time_values.data()), "Retrieving 1D time epoch values failed");
    } else {
        // Read first catchment slice from (catchment, time)
        std::vector<size_t> start = {0, 0};
        std::vector<size_t> count = {1, time_count};
        NC_CHECK(nc_get_vara_double(ncid_, varid_, start.data(), count.data(), time_values.data()), "Retrieving 1D time epoch values failed");
    }
    return time_values;
}

int NetCDFVar::get_int_value_at_index(const std::vector<size_t>& index) const {
    int value = -1;
    if (index.empty()) {
        return value;
    }
    NC_CHECK(nc_get_var1_int(ncid_, varid_, index.data(), &value), "Retrieving integer value failed");
    return value;
}

int64_t NetCDFVar::get_int64_value_at_index(const std::vector<size_t>& index) const {
    int64_t value = -1;
    if (index.empty()) {
        return value;
    }
    NC_CHECK(nc_get_var1_longlong(ncid_, varid_, index.data(), (long long*)&value), "Retrieving integer value failed");
    return value;
}

double NetCDFVar::get_dbl_value_at_index(const std::vector<size_t>& index) const {
    double value = -1;
    if (index.empty()) {
        return value;
    }
    NC_CHECK(nc_get_var1_double(ncid_, varid_, index.data(), &value), "Retrieving double value failed");
    return value;
}

std::string NetCDFVar::get_str_value_at_index(const std::vector<size_t>& index) const {
    char* value = nullptr;
    std::string str_value;
    NC_CHECK(nc_get_var1_string(ncid_, varid_, index.data(), &value), "Retrieving string value failed");
    if (value != nullptr) {
        str_value = std::string(value);
        NC_CHECK(nc_free_string(1, &value), "Memory free up failed");  // free allocated string
    }
    return str_value;
}

void NetCDFVar::add_attribute(const std::string& att_name, const std::string& att_value, bool write_to_file) {
    if(write_to_file){
        NC_CHECK(nc_put_att_text(ncid_, varid_, att_name.c_str(), att_value.size(), att_value.c_str()), "Writing string attribute failed");
    }
    attributes_str_[att_name] = att_value;
}

void NetCDFVar::add_attribute(const std::string& att_name, int att_value, bool write_to_file) {
    if(write_to_file){
        NC_CHECK(nc_put_att_int(ncid_, varid_, att_name.c_str(), NC_INT, 1, &att_value), "Writing integer attribute failed");
    }
    attributes_int_[att_name] = att_value;
}

void NetCDFVar::add_attribute(const std::string& att_name, double att_value, bool write_to_file) {
    if(write_to_file){
        NC_CHECK(nc_put_att_double(ncid_, varid_, att_name.c_str(), NC_DOUBLE, 1, &att_value), "Writing double attribute failed");
    }
    attributes_double_[att_name] = att_value;
}

// Access attribute values from object
std::string NetCDFVar::get_string_attribute(const std::string& att_name) const {
    auto it = attributes_str_.find(att_name);
    if(it != attributes_str_.end()) return it->second;
    LOG("String attribute value not found for: " + att_name, LogLevel::FATAL);
    throw std::runtime_error("String attribute not found: " + att_name);
}

int NetCDFVar::get_int_attribute(const std::string& att_name) const {
    auto it = attributes_int_.find(att_name);
    if(it != attributes_int_.end()) return it->second;
    LOG("Integer type attribute value not found for: " + att_name, LogLevel::FATAL);
    throw std::runtime_error("Integer attribute not found: " + att_name);
}

double NetCDFVar::get_double_attribute(const std::string& att_name) const {
    auto it = attributes_double_.find(att_name);
    if(it != attributes_double_.end()) return it->second;
    LOG("Double type attribute value not found: " + att_name, LogLevel::FATAL);
    throw std::runtime_error("Double attribute not found: " + att_name);
}

size_t NetCDFVar::get_catchment_index(const int64_t& catchment_id) const
{
    auto it = catchment_index_.find(catchment_id);
    if (it == catchment_index_.end()) {
        LOG("Variable not found in NetCDF: " + std::to_string(catchment_id), LogLevel::FATAL);
        throw std::runtime_error(std::string("Variable not found in NetCDF: ") + std::to_string(catchment_id));
    }
    return it->second;
}

void NetCDFVar::build_catchments_index(size_t num_items)
{
    std::vector<int64_t> data(num_items);
    NC_CHECK(nc_get_var_longlong(ncid_, varid_, (long long*)data.data()), "Building variables index failed");
    for (size_t index = 0; index < num_items; ++index) {
        int64_t key = data[index];
        catchment_index_[key] = index;
    }
}

void NetCDFVar::read_slice(const std::vector<size_t>& start, const std::vector<size_t>& count, double* data) const
{
    NC_CHECK(nc_get_vara_double(ncid_, varid_, start.data(), count.data(), data), "Reading double data from slice failed");
}

void NetCDFVar::write_int_1d(const std::vector<int>& data) const {
    std::vector<size_t> index(1);
    for (size_t i = 0; i < data.size(); ++i) {
        index[0] = i;
        NC_CHECK(nc_put_var1_int(ncid_, varid_, index.data(), &data[i]), "Writing single integer value in NetCDF failed");
    }
}

void NetCDFVar::write_flattened_double_array(const std::vector<double>& data) const {
    size_t rank = dim_ids_.size();
    std::vector<size_t> md_index(rank);
    for (size_t i = 0; i < data.size(); ++i) {
        size_t linear = i;
        // Convert linear index to multi-dimensional index
        for (int d = rank - 1; d >= 0; --d) {
            md_index[d] = linear % get_dim_size(dim_ids_[d]);
            linear /= get_dim_size(dim_ids_[d]);
        }
        NC_CHECK(nc_put_var1_double(ncid_, varid_, md_index.data(), &data[i]), "Writing single integer value in NetCDF failed");
    }
}
#endif // NGEN_WITH_NETCDF
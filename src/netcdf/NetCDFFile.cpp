#if NGEN_WITH_MPI
    #define _PARALLEL4
#endif

#if NGEN_WITH_NETCDF
#include "NetCDFFile.hpp"
#include "ewts_ngen/logger.hpp"
#include <netcdf.h>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <NGenConfig.h>


NetCDFFile::NetCDFFile(const std::string& filename, bool write_only, bool is_mpi)
    : nc_file_name_(filename), is_mpi_(is_mpi)
{
    int retval;
    int mode = NC_NETCDF4;
#if NGEN_WITH_MPI
    if(is_mpi_){
        if(write_only){
            retval = nc_create_par(nc_file_name_.c_str(), NC_NETCDF4 | NC_MPIIO | NC_CLOBBER,
                MPI_COMM_WORLD, MPI_INFO_NULL, &ncid_);
        }
        else{
            retval = nc_open_par(nc_file_name_.c_str(), NC_WRITE | NC_MPIIO,
                MPI_COMM_WORLD, MPI_INFO_NULL, &ncid_);
        }
    }
    else
#endif
    {
        if(write_only){
            LOG("Attempting to create NetCDF file", LogLevel::DEBUG);
            retval = nc_create(nc_file_name_.c_str(), NC_NETCDF4 | NC_CLOBBER, &ncid_);
        }
        else{
            read_only_ = true;
            LOG("Attempting to open NetCDF file", LogLevel::DEBUG);            
            retval = nc_open(nc_file_name_.c_str(), NC_NOWRITE, &ncid_);
        }
    }

    if(retval){
        throw std::runtime_error("Failed creating/opening NetCDF file: " + std::string(nc_strerror(retval)));
    }
}

// Add a new dimension
int NetCDFFile::add_dimension(const std::string& name, size_t len) {
    int dimid;
    int retval = nc_def_dim(ncid_, name.c_str(), len, &dimid);
    if (retval) throw std::runtime_error("Error defining dimension " + name + ": " + nc_strerror(retval));
    dims_[name] = dimid;
    return dimid;
}

void NetCDFFile::add_variable(const std::string& name, nc_type type, const std::vector<int>& dims, const std::vector<std::string>& dim_names) {
    int varid;
    int retval = nc_def_var(ncid_, name.c_str(), type, dims.size(), dims.data(), &varid);
    if (retval != NC_NOERR) throw std::runtime_error(nc_strerror(retval));
    // set fill value and missing value for NC_DOUBLE
    if (type == NC_DOUBLE) {
        double fill_missing_val = -1.0;
        nc_def_var_fill(ncid_, varid, 0, &fill_missing_val); 
        nc_put_att_double(ncid_, varid, "missing_value", NC_DOUBLE, 1, &fill_missing_val);
    }
    auto var = std::make_shared<NetCDFVar>(name, type, dims, dim_names, varid, ncid_);
    add_ncvar(var);
}

// Get dimension length by name
size_t NetCDFFile::get_dim_size(const std::string& name) const {
    auto it = dims_.find(name);
    if(it == dims_.end()) return -1;

    size_t len;
    int retval = nc_inq_dimlen(ncid_, it->second, &len);
    if(retval != NC_NOERR) return -1;
    return len;
}

int NetCDFFile::get_dim_id(const std::string& name) const {
    auto it = dims_.find(name);
    if(it == dims_.end()) return -1;
    return it->second;
}

// Add a variable
void NetCDFFile::add_ncvar(std::shared_ptr<NetCDFVar> var) {
    variables_map_[var->get_name()] = var;
}

void NetCDFFile::write_catchment_output_data(const std::string& name, std::vector<size_t> start,
                        std::vector<size_t> count, const double& data)
{
    auto var = get_ncvar_by_name(name);
    if (!var) throw std::runtime_error("Variable not found: " + name);
    int retval = nc_put_vara_double(ncid_, var->get_varid(), start.data(), count.data(), &data);
    LOG("Added value for catchments", LogLevel::INFO);
    if (retval != NC_NOERR) {
        throw std::runtime_error("Error writing value: " + std::string(nc_strerror(retval)));
    }
}

int NetCDFFile::write_data_to_ncvar(int ncid_, int varid, const std::vector<size_t>& start, 
                 const std::vector<size_t>& count, const std::vector<double>& data) {
    return nc_put_vara_double(ncid_, varid, start.data(), count.data(), data.data());
}

int NetCDFFile::write_data_to_ncvar(int ncid_, int varid, const std::vector<size_t>& start, 
                 const std::vector<size_t>& count, const std::vector<int>& data) {
    return nc_put_vara_int(ncid_, varid, start.data(), count.data(), data.data());
}

int NetCDFFile::write_data_to_ncvar(int ncid_, int varid, const std::vector<size_t>& start, 
                 const std::vector<size_t>& count, const std::vector<std::string>& data) {
    std::vector<const char*> cstrs(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        cstrs[i] = data[i].c_str();
    }
    return nc_put_vara_string(ncid_, varid, start.data(), count.data(), cstrs.data());
}

template<typename T>
void NetCDFFile::write_variable_data(const std::string& name, const std::vector<T>& data, size_t start_index) {
    auto var = get_ncvar_by_name(name);
    if (!var) throw std::runtime_error("Variable not found: " + name);
    std::vector<size_t> start = {start_index};
    std::vector<size_t> count = {data.size()};

    #if NGEN_WITH_MPI
        if (comm_ != MPI_COMM_NULL) {
            int retval = nc_var_par_access(ncid_, var->get_varid(), NC_COLLECTIVE);
            if (retval != NC_NOERR) throw std::runtime_error(nc_strerror(retval));
        }
    #endif

    int retval = NC_NOERR;
    retval = write_data_to_ncvar(ncid_, var->get_varid(), start, count, data);
    if (retval != NC_NOERR) throw std::runtime_error(nc_strerror(retval));

    // We need to construct a map of variable value and the index in the nc file.
    // this will be used while writing the output values.
    // We need this only for the coordinates variables like catchments.
    nc_type data_type;
    retval = nc_inq_vartype(ncid_, var->get_varid(), &data_type);
    if (retval != NC_NOERR) throw std::runtime_error(nc_strerror(retval));

    if (data_type == NC_STRING) {
        var->build_variables_index(data.size());
    }
}

void NetCDFFile::write_attribute_to_ncvar(const std::string& name, const std::string& attName, const std::string& attValue) {
    auto var = get_ncvar_by_name(name);
    if (!var) throw std::runtime_error("Variable not found: " + name);
    double value;
    size_t pos;
    bool is_numeric = true;
    try {
        value = std::stod(attValue, &pos);
    }
    catch (...) {
        is_numeric = false;
    }
    if (pos == attValue.size()){ //needed to ensure that the entire string is a numeric value.
        var->add_attribute(attName, value);
    }
    else{
        var->add_attribute(attName, attValue);
    }
}

std::shared_ptr<NetCDFVar> NetCDFFile::get_ncvar(const std::string& name) const
{
    return get_ncvar_by_name(name);
}

void NetCDFFile::end_def_mode()
{
    int retval = nc_enddef(ncid_);
    if(retval != NC_NOERR)
        throw std::runtime_error(std::string("Error switching to NetCDF data mode: ") + nc_strerror(retval));
}

std::shared_ptr<NetCDFVar> NetCDFFile::get_ncvar_by_name(const std::string& name) const {
    auto it = variables_map_.find(name);
    if (it != variables_map_.end()) return it->second;
    return nullptr;
}

void NetCDFFile::load_variables()
{
    int retval;
    int mode = read_only_ ? NC_NOWRITE : NC_WRITE;

    if (retval != NC_NOERR) {
        throw std::runtime_error(std::string("Failed to open NetCDF file: ") + nc_strerror(retval));
    }

    // Load dimensions
    int num_dims;
    retval = nc_inq_ndims(ncid_, &num_dims);
    if (retval != NC_NOERR)
        throw std::runtime_error(std::string("Number of Dimensions: ") + nc_strerror(retval));

    for (int i = 0; i < num_dims; ++i) {
        char dim_name[NC_MAX_NAME + 1];
        size_t dim_len;
        retval = nc_inq_dim(ncid_, i, dim_name, &dim_len);
        if (retval == NC_NOERR) {
            dims_[dim_name] = i;
        }
    }

    // Load variables
    int num_variables;
    retval = nc_inq_nvars(ncid_, &num_variables);
    if (retval != NC_NOERR)
        throw std::runtime_error(std::string("Number of Variables: ") + nc_strerror(retval));

    for (int i = 0; i < num_variables; ++i) {
        char var_name[NC_MAX_NAME + 1];
        retval = nc_inq_varname(ncid_, i, var_name);
        if (retval != NC_NOERR) continue;

        nc_type type;
        int num_dims_var;
        int dim_ids[NC_MAX_DIMS];

        retval = nc_inq_var(ncid_, i, nullptr, &type, &num_dims_var, dim_ids, nullptr);
        if (retval != NC_NOERR) continue;

        // Collect dim names
        std::vector<std::string> dim_names;
        std::vector<int> dim_ids_var;
        for (int d = 0; d < num_dims_var; ++d) {
            dim_ids_var.push_back(dim_ids[d]);
            char dim_name[NC_MAX_NAME + 1];
            nc_inq_dim(ncid_, dim_ids[d], dim_name, nullptr);
            dim_names.push_back(dim_name);
        }

        // Create shared_ptr NcVar
        auto nc_var = std::make_shared<NetCDFVar>(var_name, type,dim_ids_var, dim_names, i, ncid_);
        variables_.push_back(nc_var);
        variables_map_[var_name] = nc_var;
    }
}

std::vector<std::string> NetCDFFile::list_variables() const
{
    std::vector<std::string> names;
    for(const auto& var : variables_) {
        names.push_back(var->get_name());
    }
    return names;
}

void NetCDFFile::close_file() {
    if (ncid_ >= 0) nc_close(ncid_);
}

NetCDFFile::~NetCDFFile() {
    close_file();
}

template void NetCDFFile::write_variable_data(const std::string& name, const std::vector<double>& data, size_t start_index);
template void NetCDFFile::write_variable_data(const std::string& name, const std::vector<int>& data, size_t start_index);
template void NetCDFFile::write_variable_data(const std::string& name, const std::vector<std::string>& data, size_t start_index);
//template void NetCDFFile::writeAllVariablesDistributed<double>(const std::vector<std::vector<double>>&);
//template void NetCDFFile::writeAllVariablesDistributed<int>(const std::vector<std::vector<int>>&);
#endif // NGEN_WITH_NETCDF
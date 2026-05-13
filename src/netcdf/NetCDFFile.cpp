#if NGEN_WITH_MPI
    #define _PARALLEL4
#endif

#if NGEN_WITH_NETCDF
#include "NetCDFFile.hpp"
#include <netcdf.h>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <NGenConfig.h>

#if NGEN_WITH_MPI
NetCDFFile::NetCDFFile(const std::string& filename, bool write_only, MPI_Comm comm)
    : nc_file_name_(filename), comm_(comm)
{
    int retval;
    int mode = NC_NETCDF4;
    if (write_only) mode |= NC_CLOBBER;

    if ((retval = nc_create_par(filename.c_str(), mode, comm_, MPI_INFO_NULL, &ncid_)))
        throw std::runtime_error("Error creating NetCDF file: " + std::string(nc_strerror(retval)));
}
#else
NetCDFFile::NetCDFFile(const std::string& full_filename, bool write_only)
{
    int retval;
    int mode = NC_NETCDF4;
    if (write_only) mode |= NC_CLOBBER;
    if ((retval = nc_create(full_filename.c_str(), mode, &ncid_)))
        throw std::runtime_error("Error creating NetCDF file: " + std::string(nc_strerror(retval)));
    
}
#endif

NetCDFFile::NetCDFFile(const std::string &filename)
{
    nc_file_name_ = filename;
    read_only_ = true;
#if NGEN_WITH_MPI
    comm_ = MPI_COMM_NULL;
#endif
    load_variables();
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
    if (type = NC_DOUBLE) {
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
    if(it == dims_.end()) throw std::runtime_error("Dimension not found: " + name);

    size_t len;
    int retval = nc_inq_dimlen(ncid_, it->second, &len);
    if(retval) throw std::runtime_error("Error getting dimension length for " + name + ": " + nc_strerror(retval));
    return len;
}

int NetCDFFile::get_dim_id(const std::string& name) const {
    auto it = dims_.find(name);
    if(it == dims_.end()) throw std::runtime_error("Dimension not found: " + name);
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
            int retval = nc_var_par_access(ncid_, var->getVarId(), NC_COLLECTIVE);
            if (retval != NC_NOERR) throw std::runtime_error(nc_strerror(retval));
        }
    #endif

    int retval = NC_NOERR;
    retval = write_data_to_ncvar(ncid_, var->get_varid(), start, count, data);

    if (retval != NC_NOERR) throw std::runtime_error(nc_strerror(retval));
}

void NetCDFFile::write_attribute_to_ncvar(const std::string& name, const std::string& attName, const std::string& attValue) {
    auto var = get_ncvar_by_name(name);
    if (!var) throw std::runtime_error("Variable not found: " + name);
    var->add_attribute(attName, attValue);
}

// template<typename T>
// void NcFile::writeAllVariables(const std::vector<std::vector<T>>& all_data) {
//     if (all_data.size() != variables_.size())
//         throw std::runtime_error("Mismatch between variables and data");

//     for (size_t v = 0; v < variables_.size(); ++v) {
//         auto var = variables_[v];
//         const auto& data = all_data[v];

//     #if NGEN_WITH_MPI
//             if (comm_ != MPI_COMM_NULL && num_procs_ > 1) {
//                 int retval = nc_var_par_access(ncid_, var->getVarId(), NC_COLLECTIVE);
//                 if (retval != NC_NOERR) throw std::runtime_error(nc_strerror(retval));
//             }
//     #endif

//         int retval = NC_NOERR;
//         if constexpr (std::is_same_v<T, double>) {
//             retval = nc_put_var_double(ncid_, var->getVarId(), data.data());
//         } else if constexpr (std::is_same_v<T, int>) {
//             retval = nc_put_var_int(ncid_, var->getVarId(), data.data());
//         } else {
//             static_assert(sizeof(T) == 0, "Unsupported data type");
//         }

//         if (retval != NC_NOERR) throw std::runtime_error(nc_strerror(retval));
//     }
// }

// template<typename T>
// void NetCDFFile::writeAllVariablesDistributed(const std::vector<std::vector<T>>& all_data) {
//     if (all_data.size() != variables_.size())
//         throw std::runtime_error("Mismatch between variables and data");

//     for (size_t v = 0; v < variables_.size(); ++v) {
//         auto var = variables_[v];
//         const auto& data = all_data[v];

//         // Compute first-dimension chunking
//         size_t first_dim_size = var->getDims()[0];
//         size_t start0, count0;
//         computeStartCount(first_dim_size, start0, count0);

//         std::vector<size_t> start(var->getDims().size(), 0);
//         std::vector<size_t> count = var->getDims();
//         start[0] = start0;
//         count[0] = count0;

// #if NGEN_WITH_MPI
//         if (comm_ != MPI_COMM_NULL && num_procs_ > 1) {
//             int retval = nc_var_par_access(ncid_, var->getVarId(), NC_COLLECTIVE);
//             if (retval != NC_NOERR) throw std::runtime_error(nc_strerror(retval));
//         }
// #endif

//         int retval = NC_NOERR;
//         if constexpr (std::is_same_v<T, double>) {
//             retval = nc_put_vara_double(ncid_, var->getVarId(), start.data(), count.data(), 
//                                         &data[start0 * (data.size()/first_dim_size)]);
//         } else if constexpr (std::is_same_v<T, int>) {
//             retval = nc_put_vara_int(ncid_, var->getVarId(), start.data(), count.data(), 
//                                      &data[start0 * (data.size()/first_dim_size)]);
//         } else {
//             static_assert(sizeof(T) == 0, "Unsupported data type");
//         }

//         if (retval != NC_NOERR) throw std::runtime_error(nc_strerror(retval));
//     }
// }

std::shared_ptr<NetCDFVar> NetCDFFile::get_ncvar_by_name(const std::string& name) {
    auto it = variables_map_.find(name);
    if (it != variables_map_.end()) return it->second;
    return nullptr;
}

// void NetCDFFile::computeStartCount(size_t total_size, size_t& start, size_t& count) {
// #if NGEN_WITH_MPI
//     if (comm_ == MPI_COMM_NULL || num_procs_ == 1) {
//         start = 0;
//         count = total_size;
//     } else {
//         size_t chunk = total_size / num_procs_;
//         start = rank_ * chunk;
//         count = (rank_ == num_procs_ - 1) ? (total_size - start) : chunk;
//     }
// #else
//     start = 0;
//     count = total_size;
// #endif
// }

void NetCDFFile::load_variables()
{
    int retval;

#if NGEN_WITH_MPI
    if(comm_ != MPI_COMM_NULL) {
        if((retval = nc_open_par(ncFileName.c_str(), NC_NOWRITE, comm_, MPI_INFO_NULL, &ncid_))) {
            throw std::runtime_error("Cannot open NetCDF (MPI): " + std::string(nc_strerror(retval)));
        }
    } else
#endif
    {
        if((retval = nc_open(nc_file_name_.c_str(), NC_NOWRITE, &ncid_))) {
            throw std::runtime_error("Cannot open NetCDF: " + std::string(nc_strerror(retval)));
        }
    }

    int num_variables;
    if((retval = nc_inq_nvars(ncid_, &num_variables))) {
        nc_close(ncid_);
        throw std::runtime_error("Cannot get number of variables: " + std::string(nc_strerror(retval)));
    }

    variables_.clear();

    for(int varid = 0; varid < num_variables; ++varid) {
        char name[NC_MAX_NAME + 1];
        nc_type type;
        int num_dims, dim_ids[NC_MAX_VAR_DIMS], num_attributes;

        if((retval = nc_inq_var(ncid_, varid, name, &type, &num_dims, dim_ids, &num_attributes))) {
            std::cerr << "Warning: could not get var info for varid " << varid
                      << ": " << nc_strerror(retval) << std::endl;
            continue;
        }

        std::vector<int> dims(num_dims);
        std::vector<std::string> dim_names;
        for(int d=0; d<num_dims; ++d) {
            size_t len;
            char dim_name[NC_MAX_NAME + 1];
            if((retval = nc_inq_dim(ncid_, dim_ids[d], dim_name, &len))) {
                std::cerr << "Warning: could not get dim length for dimid " << dim_ids[d]
                          << ": " << nc_strerror(retval) << std::endl;
                len = 0;
            }
            dims[d] = len;
            dim_names.push_back(dim_name);
        }

        auto var = std::make_shared<NetCDFVar>(name, type, dims, dim_names, varid, ncid_);

        // load attributes
        for(int attid=0; attid<num_attributes; ++attid) {
            char attname[NC_MAX_NAME + 1];

            if((retval = nc_inq_attname(ncid_, varid, attid, attname))) continue;

            nc_type att_type;
            size_t att_len;
            if((retval = nc_inq_att(ncid_, varid, attname, &att_type, &att_len))) continue;

            if(att_type == NC_CHAR) {
                
                nc_inq_attlen(ncid_, varid, attname, &att_len);
                std::vector<char> buffer(att_len + 1, '\0');
                nc_get_att_text(ncid_, varid, attname, buffer.data());
                var->add_attribute(attname, std::string(buffer.data(), att_len));
            } else if(att_type == NC_INT) {
                int val;
                nc_get_att_int(ncid_, varid, attname, &val);
                var->add_attribute(attname, val);
            } else if(att_type == NC_DOUBLE) {
                double val;
                nc_get_att_double(ncid_, varid, attname, &val);
                var->add_attribute(attname, val);
            }
            // extend for other types if needed (float, long, etc.)
        }

        variables_.push_back(var);
    }
    nc_close(ncid_);
}

std::vector<std::string> NetCDFFile::list_variables() const
{
    std::vector<std::string> names;
    for(const auto& var : variables_) {
        names.push_back(var->get_name());
    }
    return names;
}

std::shared_ptr<NetCDFVar> NetCDFFile::get_ncvar(const std::string& name) const
{
    for(const auto& var : variables_) {
        if(var->get_name() == name) return var;
    }
    return nullptr;
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
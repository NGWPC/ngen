#ifndef NETCDFVAR_HPP
#define NETCDFVAR_HPP

//#include <NGenConfig.h>

#if NGEN_WITH_NETCDF
#include <string>
#include <stdexcept>
#include <vector>
#include <map>
#include <unordered_map>

#if NGEN_WITH_MPI
#include <mpi.h>
#include <netcdf_par.h>
#else
#include <netcdf.h>
#endif

class NetCDFVar {
public:
    NetCDFVar(const std::string& name, nc_type type, const std::vector<int>& dims, 
        const std::vector<std::string>& dim_names, int varid, int ncid);

    const std::string& get_name() const;
    nc_type get_type() const;
    const std::vector<int>& get_dims() const;
    const std::vector<std::string>& get_dim_names() const;
    size_t get_dim_size(const std::string& dim_name) const;
    size_t get_dim_size(size_t idx) const;
    size_t get_dim_count() const;
    int get_varid() const;
    size_t get_total_size() const;
    std::vector<std::string> get_string_array_values() const;
    std::vector<double> get_time_values() const;
    int get_int_value_at_index(const std::vector<size_t>& index) const;
    double get_dbl_value_at_index(const std::vector<size_t>& index) const;
    std::string get_str_value_at_index(const std::vector<size_t>& index) const;
    void read_slice(const std::vector<size_t>& start, const std::vector<size_t>& count, double* data) const;

    void add_attribute(const std::string& att_name, const std::string& att_value);
    void add_attribute(const std::string& att_name, int att_value);
    void add_attribute(const std::string& att_name, double att_value);
    void build_variables_index(size_t num_items);

    std::vector<std::string> list_attributes() const;
    std::string get_string_attribute(const std::string& att_name) const;
    int get_int_attribute(const std::string& att_name) const;
    double get_double_attribute(const std::string& att_name) const;
    size_t get_variable_index(const std::string& name) const;

    void write_timesliced_data(size_t timestep, size_t slice_start, size_t slice_count, const double* data);
    
    //Implementing this single-element write function for mdframe_netcdf_test
    template<typename T>
    void write_single_value(const std::vector<size_t>& block_array, const T& value) const 
    {
        std::vector<size_t> start(block_array.begin(), block_array.end());
        std::vector<size_t> count(block_array.size(), 1);

        int retval = 0;

        if (std::is_same<T,int>::value) {
            retval = nc_put_vara_int(ncid_, varid_, start.data(), count.data(), &value);
        }
        else if (std::is_same<T,float>::value) {
            retval = nc_put_vara_float(ncid_, varid_, start.data(), count.data(), &value);
        }
        else if (std::is_same<T,double>::value) {
            retval = nc_put_vara_double(ncid_, varid_, start.data(), count.data(), &value);
        }
        else {
            throw std::runtime_error("Unsupported type for write_single_value");
        }

        if (retval != NC_NOERR) throw std::runtime_error(std::string("Erorr in writing single value: ") + nc_strerror(retval));
    }

    private:
    std::string name_;
    nc_type type_;
    std::vector<int> dims_;
    std::vector<std::string> dim_names_;
    int varid_;
    int ncid_;

    std::map<std::string, std::string> attributes_str_;
    std::map<std::string, int> attributes_int_;
    std::map<std::string, double> attributes_double_;
    std::unordered_map<std::string, size_t> variable_index_;
};
#endif // NGEN_WITH_NETCDF
#endif // NETCDFVAR_HPP
#ifndef NETCDFVAR_HPP
#define NETCDFVAR_HPP

//#include <NGenConfig.h>

#if NGEN_WITH_NETCDF
#include <string>
#include <stdexcept>
#include <vector>
#include <map>
#include <unordered_map>
#include <netcdf.h>
#endif

#if NGEN_WITH_MPI
#include <mpi.h>
// #include <netcdf_par.h>
#endif

class NetCDFVar {
public:
    NetCDFVar(const std::string& name, nc_type type, const std::vector<int>& dim_ids, 
        const std::vector<std::string>& dim_names, int varid, int ncid);

    const std::string& get_name() const;
    nc_type get_type() const;
    const std::vector<int>& get_dims() const;
    const std::vector<std::string>& get_dim_names() const;
    size_t get_dim_size(const std::string& dim_name) const;
    size_t get_dim_size(size_t idx) const;
    size_t get_dim_count() const;
    int get_varid() const;
    std::vector<std::string> get_string_array_values() const;
    std::vector<std::string> get_int64_array_values() const;
    std::vector<double> get_time_values() const;
    int get_int_value_at_index(const std::vector<size_t>& index) const;
    int64_t get_int64_value_at_index(const std::vector<size_t>& index) const;
    double get_dbl_value_at_index(const std::vector<size_t>& index) const;
    std::string get_str_value_at_index(const std::vector<size_t>& index) const;
    void read_slice(const std::vector<size_t>& start, const std::vector<size_t>& count, double* data) const;

    void add_attribute(const std::string& att_name, const std::string& att_value, bool write_to_file = true);
    void add_attribute(const std::string& att_name, int att_value, bool write_to_file = true);
    void add_attribute(const std::string& att_name, double att_value, bool write_to_file = true);
    void build_catchments_index(size_t num_items);

    std::string get_string_attribute(const std::string& att_name) const;
    int get_int_attribute(const std::string& att_name) const;
    double get_double_attribute(const std::string& att_name) const;
    size_t get_catchment_index(const int64_t& catchment_id) const;

    //for mdframe_netcdf tests
    void write_int_1d(const std::vector<int>& data) const;
    void write_flattened_double_array(const std::vector<double>& data) const; 
    

    private:
    std::string name_;
    nc_type type_;
    std::vector<int> dim_ids_;
    std::vector<std::string> dim_names_;
    int varid_;
    int ncid_;

    std::map<std::string, std::string> attributes_str_;
    std::map<std::string, int> attributes_int_;
    std::map<std::string, double> attributes_double_;
    std::unordered_map<int64_t, size_t> catchment_index_;
};
#endif // NGEN_WITH_NETCDF
#endif // NETCDFVAR_HPP
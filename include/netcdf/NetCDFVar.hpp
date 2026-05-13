#ifndef NETCDFVAR_HPP
#define NETCDFVAR_HPP

//#include <NGenConfig.h>

#if NGEN_WITH_NETCDF
#include <string>
#include <vector>
#include <map>
#include <netcdf.h>

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
    void read_slice(const std::vector<size_t>& start, const std::vector<size_t>& count, double* data) const;

    void add_attribute(const std::string& att_name, const std::string& att_value);
    void add_attribute(const std::string& att_name, int att_value);
    void add_attribute(const std::string& att_name, double att_value);

    std::vector<std::string> list_attributes() const;
    std::string get_string_attribute(const std::string& att_name) const;
    int get_int_attribute(const std::string& att_name) const;
    double get_double_attribute(const std::string& att_name) const;

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
};
#endif // NGEN_WITH_NETCDF
#endif // NETCDFVAR_HPP
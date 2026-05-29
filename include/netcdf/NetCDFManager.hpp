#ifndef NETCDFMANAGER_HPP
#define NETCDFMANAGER_HPP

//#include <NGenConfig.h>


#if NGEN_WITH_NETCDF
#if NGEN_WITH_MPI
    #include <mpi.h>
    #define _PARALLEL4
#endif
#include "NetCDFFile.hpp"
#include "NetCDFVar.hpp"
#include <vector>
#include <string>
#include <map>
#include <stdexcept>


namespace realization { 
    class Formulation_Manager; 
    class Catchment_Formulation;
}
class Simulation_Time;

class NetCDFManager
{
public:
     NetCDFManager(std::shared_ptr<realization::Formulation_Manager> manager, 
        const std::string& output_name, Simulation_Time const& sim_time, int mpi_rank, int mpi_num_procs);

    // Constructor for read-only NetCDF (no MPI needed)
    NetCDFManager(const std::string& filename, bool read_only);

    // Default constructor for mdframe tests 
    NetCDFManager();

    // File operations
    int create_file(const std::string& filename);
    void open_file();
    void close_file();

    void gather_all_catchments(const std::vector<std::string>& catchments_in_proc);

    //set up netcdf dimensions and variables
    void define_catchment_netcdf_components();

    // List variable names
    std::vector<std::string> list_variables() const;

    //Get NetCDFFile handle
    NetCDFFile* get_file_handle() {return nc_file_.get();}

    // Access NetCDFVar by name
    std::shared_ptr<NetCDFVar> get_ncvar_by_name(const std::string& name) const;

    // Attribute access
    std::string get_string_attribute(const std::string& var_name, const std::string& att_name) const;
    int get_int_attribute(const std::string& var_name, const std::string& att_name) const;
    double get_double_attribute(const std::string& var_name, const std::string& att_name) const;

    // Add a dimension
    int add_dimension(const std::string& name, size_t len);

    // Add a variable
    void add_variable(const std::string& var_name, nc_type type, const std::vector<int>& dims, const std::vector<std::string>& dim_names);

    // Add variables to the file (for writing)
    void add_output_variable_data_from_formulation();

    // Add catchment output data to the file (for writing)
    void write_simulations_response_from_formulation(size_t time_index, std::map<std::string, std::string> catchment_output_values);
    void primary_netcdf_writer(size_t time_index, const std::map<std::string, std::string>& catchment_output_values);
    void secondary_netcdf_worker(const std::map<std::string, std::string>& catchment_output_values);

    //self-chunking and writing.
    void get_local_time_range(size_t& start, size_t& count) const;
    size_t get_chunk_start() const {return chunk_start_; }
    size_t get_chunk_count() const {return chunk_count_; }
    void prepare_data_chunks(std::map<std::string, std::string> catchment_output_vals);
    void write_timestep_data_to_netcdf(size_t time_index);

    ~NetCDFManager();

private:
    bool read_only_;
    std::string nc_filename_;
    std::unique_ptr<NetCDFFile> nc_file_;
    std::vector<NetCDFVar> vars_;
    std::shared_ptr<realization::Formulation_Manager> manager_;
    std::shared_ptr<Simulation_Time> sim_time_;
    size_t chunk_start_ = 0;
    size_t chunk_count_ = 0;
    size_t num_timesteps_;
    int num_catchments_ = 0;
    std::vector<std::string> catchments_;
    std::map<std::string, std::shared_ptr<NetCDFVar>> variables_map_;
    std::vector<std::string> nc_output_variables_;
    std::vector<std::vector<double>> data_chunks_;

#if NGEN_WITH_MPI
    MPI_Comm comm_;
#endif
    int rank_ = 0;
    int num_procs_ = 1;
    bool is_mpi_ = false;
};
#endif // NGEN_WITH_NETCDF
#endif // NETCDFMANAGER_HPP
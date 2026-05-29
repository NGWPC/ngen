#if NGEN_WITH_NETCDF
#include "NetCDFManager.hpp"
#include "Formulation_Manager.hpp"
#include "Catchment_Formulation.hpp"
#include "simulation_time/Simulation_Time.hpp"
#include "ewts_ngen/logger.hpp"
#include <stdexcept>
#include <NGenConfig.h>

NetCDFManager::NetCDFManager(std::shared_ptr<realization::Formulation_Manager> manager, 
        const std::string& output_name, Simulation_Time const& sim_time, int mpi_rank, int mpi_num_procs)
{
    manager_ = manager;
    sim_time_ = std::make_shared<Simulation_Time>(sim_time);
    nc_filename_ = manager_->get_output_root() + output_name + ".nc";
#if NGEN_WITH_MPI
    rank_ = mpi_rank;
    num_procs_ = mpi_num_procs;
    is_mpi_ = (num_procs_ > 1);
    comm_ = (num_procs_ > 1) ? MPI_COMM_WORLD : MPI_COMM_NULL;
#else
    is_mpi_ = false;
#endif

    int num_catchments = manager_->get_size();
    std::vector<std::string> catchments_in_proc; 
    catchments_in_proc.reserve(num_catchments);
    for (auto const& formulation_info : manager_->get_all_formulations())
    {
        std::string catchm = formulation_info.first;
        if (catchm.compare(0, 3, "cat") != 0) {
            catchm = "cat-" + catchm;
        }
        catchments_in_proc.push_back(catchm);
    }
    gather_all_catchments(catchments_in_proc);
    
    if (rank_ == 0){
        nc_file_ = std::make_unique<NetCDFFile>(nc_filename_, true, is_mpi_);
        define_catchment_netcdf_components();
    }
#if NGEN_WITH_MPI
    MPI_Barrier(comm_);
#endif
}

NetCDFManager::NetCDFManager(const std::string& filename, bool read_only)
    : read_only_(true)
{
    if (read_only_){
        nc_file_ = std::make_unique<NetCDFFile>(filename, !read_only, false);
    }
    else{
        throw std::runtime_error("Write only non-MPI function not implemented.");
    }
#if NGEN_WITH_MPI
    comm_ = MPI_COMM_NULL;
    rank_ = 0;
    num_procs_ = 1;
#endif
}

NetCDFManager::NetCDFManager()
{}

int NetCDFManager::create_file(const std::string& filename)
{
#if NGEN_WITH_MPI
    if (num_procs_ > 1) {
        // MPI-enabled NetCDF
        is_mpi_ = true;
        nc_file_ = std::make_unique<NetCDFFile>(filename, true, is_mpi_);
    }else{
        is_mpi_ = false;
    }
#endif
    nc_filename_ = filename;
    nc_file_ = std::make_unique<NetCDFFile>(nc_filename_, true, is_mpi_);
    if (!nc_file_) {
        throw std::runtime_error("Failed to create NetCDF file: " + filename);
    }
    return nc_file_ ->get_ncid();
}

void NetCDFManager::gather_all_catchments(const std::vector<std::string>& catchments_in_proc)
{
    if (!is_mpi_)
    {
        catchments_ = catchments_in_proc;
        return;
    }
#if NGEN_WITH_MPI
    if (rank_ == 0)
    {
        catchments_ = catchments_in_proc;
        for (int proc = 1;proc < num_procs_; ++proc)
        {
            int count;
            MPI_Recv(&count, 1, MPI_INT, proc, 100, comm_, MPI_STATUS_IGNORE);
            for (int i = 0;i < count; ++i)
            {
                int name_len;
                MPI_Recv(&name_len, 1, MPI_INT, proc, 101, comm_, MPI_STATUS_IGNORE);
                std::string catchment_name(name_len, ' ');
                MPI_Recv( &catchment_name[0], name_len, MPI_CHAR, proc, 102, comm_, MPI_STATUS_IGNORE);
                catchments_.push_back(catchment_name);
            }
        }
    }
    else
    {
        int count = catchments_in_proc.size();
        MPI_Send( &count, 1, MPI_INT, 0, 100, comm_);
        for (const auto& catchment_name : catchments_in_proc)
        {
            int name_len = catchment_name.size();
            MPI_Send( &name_len, 1, MPI_INT, 0, 101, comm_);
            MPI_Send( catchment_name.c_str(), name_len, MPI_CHAR, 0, 102, comm_);
        }
    }
#endif
}

void NetCDFManager::define_catchment_netcdf_components()
{
    std::string name;
    int dim_id;
    std::vector<int> dim_ids;
    std::vector<std::string> names;
    
    //add time dimension and variable
    try
    {
        name = "time";
        int num_timesteps = sim_time_->get_total_output_times();
        dim_id = add_dimension(name, num_timesteps);
        dim_ids = {dim_id};
        names = {name};
        add_variable(name, NC_INT, dim_ids, names);
        
        //add timestep values and attributes for time
        std::vector<int> time_epoch_seconds(num_timesteps);
        time_epoch_seconds[0] = sim_time_->get_current_epoch_time();
        for(int time_index = 1; time_index < num_timesteps; time_index++)
        {
            sim_time_->advance_timestep();
            time_epoch_seconds[time_index] = sim_time_->get_current_epoch_time();
        }
        nc_file_->write_variable_data(name, time_epoch_seconds);
        nc_file_->write_attribute_to_ncvar(name, "units", "Seconds since 1970-01-01 00:00:00");
        nc_file_->write_attribute_to_ncvar(name, "calendar", "gregorian");
    }
    catch(const std::runtime_error& e)
    {
        LOG(std::string("Error in adding time information to catchment NetCDF: ") + e.what(), LogLevel::FATAL);
        throw std::runtime_error(std::string("Error in adding time information to catchment NetCDF: ") + e.what());
    }

    //add catchments dimension and variable
    try
    {
        name = "catchments";
        int num_catchments = catchments_.size();
        dim_id = add_dimension(name, num_catchments);
        dim_ids = {dim_id};
        names = {name};
        add_variable(name, NC_STRING, dim_ids, names);
        nc_file_->write_variable_data(name, catchments_);
        nc_file_->write_attribute_to_ncvar(name, "Catchment ID", "Catchment identifier in input");

        //Compute writing chunks for catchments
        size_t base = num_catchments / num_procs_;
        size_t rem = num_catchments % num_procs_;
        chunk_count_ = base + (rank_ < rem ? 1 : 0);
        chunk_start_ = rank_ * base + std::min(static_cast<size_t>(rank_), rem);
    }
    catch(const std::runtime_error& e)
    {
        LOG(std::string("Error in adding catchment IDs to catchment NetCDF: ") + e.what(), LogLevel::FATAL);
        throw std::runtime_error(std::string("Error in adding catchment IDs to catchment NetCDF: ") + e.what());
    }

    // Add realization config output variables
    try
    {
        add_output_variable_data_from_formulation();
    }
    catch(const std::runtime_error& e)
    {
        LOG(std::string("Error in adding output variables information to catchment NetCDF: ") + e.what(), LogLevel::FATAL);
        throw std::runtime_error(std::string("Error in adding output variables information to catchment NetCDF: ") + e.what());
    }

    // Resize the 2D array/vector that will hold the output data for each timestep.
    int num_variables = nc_output_variables_.size();
    if (num_variables > 0){
        data_chunks_.resize(num_variables);
        for(size_t v = 0; v < num_variables; ++v)
        {
            data_chunks_[v].resize(chunk_count_);
        }
    }

    // Switch to data mode for writing
    nc_file_->end_def_mode();
}

int NetCDFManager::add_dimension(const std::string& name, size_t len){
    return nc_file_->add_dimension(name, len);
}

void NetCDFManager::add_variable(const std::string& var_name, nc_type type, const std::vector<int>& dim_ids, const std::vector<std::string>& dim_names){
    nc_file_->add_variable(var_name, type, dim_ids, dim_names);
}

void NetCDFManager::add_output_variable_data_from_formulation() 
{
    typename std::map<std::string, std::shared_ptr<realization::Catchment_Formulation>>::const_iterator it = manager_->begin();
    const auto& catchment_info = *it;
    auto r_c = std::dynamic_pointer_cast<realization::Bmi_Formulation>(catchment_info.second);
    if(r_c->get_output_header_count() > 0){
        std::vector<std::string>output_variables = r_c->get_output_variable_names();
        std::vector<std::string>output_headers = r_c->get_output_header_fields();
        std::vector<std::string>output_units = r_c->get_output_variable_units();
        nc_output_variables_.reserve(output_variables.size());
        std::vector<int> dims_2D = {nc_file_->get_dim_id("time"), nc_file_->get_dim_id("catchments")};
        std::vector<std::string> dim_names{"time", "catchments"};

        for(int index = 0; index < output_variables.size(); index ++){
            nc_file_->add_variable(output_headers[index], NC_DOUBLE, dims_2D, dim_names);
            nc_file_->write_attribute_to_ncvar(output_headers[index], "variable name", output_variables[index]);
            nc_file_->write_attribute_to_ncvar(output_headers[index], "variable units", output_units[index]);
            nc_file_->write_attribute_to_ncvar(output_headers[index], "_FillValue", "-1.0");
            nc_file_->write_attribute_to_ncvar(output_headers[index], "missing_value", "-1.0");
            nc_output_variables_.push_back(output_headers[index]);
        }
    }
}

static std::vector<double> string_split(std::string str, char delimiter)
{
    std::stringstream ss(str);
    std::vector<double> res;
    std::string token;
    while (getline(ss, token, delimiter)) { //will return full string if no comma (or single output variable).
        res.push_back(std::stod(token)); 
    }
    return res;
}

void NetCDFManager::prepare_data_chunks(std::map<std::string, std::string> catchment_output_vals)
{
    int num_variables = data_chunks_.size();
    if (num_variables > 0){ // if number of output variables is greater than zero
        size_t index = 0;
        for(auto it = catchment_output_vals.begin(); it != catchment_output_vals.end(); ++it)
        {
            if(index < chunk_start_) { index++; continue; }        // skip catchments not for this rank
            if(index >= chunk_start_ + chunk_count_) break;       // done with this rank.
            std::vector<double> catchment_output = string_split(it->second, ',');
            for(size_t var_index = 0; var_index < num_variables; ++var_index){
                data_chunks_[var_index][index - chunk_start_] = catchment_output[var_index];
            }
            index++;
        }
    }
}

void NetCDFManager::write_simulations_response_from_formulation(size_t time_index, std::map<std::string, std::string> catchment_output_values)
{
    try{
        if(!is_mpi_){
            auto var = nc_file_->get_ncvar("catchments");
            if (!var){
                LOG("Catchments variable/dimension not found in NetCDF", LogLevel::FATAL);
                throw std::runtime_error("Catchments variable/dimension not found");
            } 
            for (auto const& catchment_val : catchment_output_values)
            {
                std::string catchment_id = catchment_val.first;
                size_t catchment_index = var->get_variable_index(catchment_id);
                if (catchment_index < 0) {
                    LOG("Catchments not found in NetCDF", LogLevel::FATAL);
                    throw std::runtime_error("Catchment not found in NetCDF: " + catchment_id);
                }
                std::vector<double> catchment_output = string_split(catchment_val.second, ',');
                std::vector<size_t> start = {time_index, catchment_index};
                std::vector<size_t> count = {1, 1};
                
                for(int var_index = 0; var_index < nc_output_variables_.size(); ++var_index)
                {
                    nc_file_ ->write_catchment_output_data(nc_output_variables_[var_index], start, count, catchment_output[var_index]);
                }
            }
            nc_sync(nc_file_->get_ncid()); 
            return;
        }
    }
    catch(const std::runtime_error& e)
    {
        LOG(std::string("Error in writing simulation response to catchment NetCDF: ") + e.what(), LogLevel::FATAL);
        throw std::runtime_error(std::string("Error in writing simulation response to catchment NetCDF: ") + e.what());
    }
#if NGEN_WITH_MPI
    try{
        if (is_mpi_){
            if (rank_ == 0){
                primary_netcdf_writer(time_index, catchment_output_values);
            }
            else{
                secondary_netcdf_worker(catchment_output_values);
            }
            MPI_Barrier(comm_);
        }
    }
    catch(const std::runtime_error& e)
    {
        LOG(std::string("Error in writing simulation response to catchment NetCDF: ") + e.what(), LogLevel::FATAL);
        throw std::runtime_error(std::string("Error in writing simulation response to catchment NetCDF: ") + e.what());
    }
#endif
}

void NetCDFManager::primary_netcdf_writer(size_t time_index, const std::map<std::string, std::string>& catchment_output_values)
{
    // Lambda helper function for writing the data to netcdf file
    auto write_data_to_netcdf = [&](const std::string& catchment_id, const std::string& csv_output_line) {
        std::vector<double> catchment_output = string_split(csv_output_line, ',');
        auto var = nc_file_->get_ncvar("catchments");
        if (!var){
            LOG("Catchments variable/dimension not found in NetCDF", LogLevel::FATAL);
            throw std::runtime_error("Catchments variable/dimension not found");
        }
        size_t catchment_index = var->get_variable_index(catchment_id);
        if (catchment_index < 0) {
            LOG("Catchments not found in NetCDF", LogLevel::FATAL);
            throw std::runtime_error("Catchment not found in NetCDF: " + catchment_id);
        }
        std::vector<size_t> start = {time_index, catchment_index};
        std::vector<size_t> count = {1, 1};
        for(int var_index = 0; var_index < nc_output_variables_.size(); ++var_index)
        {
            nc_file_ ->write_catchment_output_data(nc_output_variables_[var_index], start, count, catchment_output[var_index]);
        }
    };
    //Rank 0 writer
    for (auto const& catchment_val : catchment_output_values)
    {
        std::string catchment_id = catchment_val.first;
        write_data_to_netcdf(catchment_id, catchment_val.second);
    }

#if NGEN_WITH_MPI
    // Other ranks - data sender to rank 0
    for (int proc = 1; proc < num_procs_; ++proc) {
        int catchment_count = 0;
        MPI_Recv(&catchment_count, 1, MPI_INT, proc, 200, comm_, MPI_STATUS_IGNORE);
        for (int i = 0; i < catchment_count; ++i) {
            int data_len[2] = {0, 0};
            MPI_Recv(data_len, 2, MPI_INT, proc, 201, comm_, MPI_STATUS_IGNORE);
            std::string catchment_name(data_len[0], ' ');
            std::string csv_data(data_len[1], ' ');
            MPI_Recv(&catchment_name[0], data_len[0], MPI_CHAR, proc, 202, comm_, MPI_STATUS_IGNORE);
            MPI_Recv(&csv_data[0], data_len[1], MPI_CHAR, proc, 203, comm_, MPI_STATUS_IGNORE);
            write_data_to_netcdf(catchment_name, csv_data);
        }
    }
#endif
    nc_sync(nc_file_->get_ncid()); 
}

#if NGEN_WITH_MPI
void NetCDFManager::secondary_netcdf_worker(const std::map<std::string, std::string>& catchment_output_values) {
    int catchment_count = catchment_output_values.size();
    MPI_Send(&catchment_count, 1, MPI_INT, 0, 200, comm_);
    for (const auto& [catchment_name, csv_data] : catchment_output_values) {
        int data_len[2] = {catchment_name.size(), csv_data.size()};
        MPI_Send(data_len, 2, MPI_INT, 0, 201, comm_);
        MPI_Send(const_cast<char*>(catchment_name.data()), data_len[0], MPI_CHAR, 0, 202, comm_);
        MPI_Send(const_cast<char*>(csv_data.data()), data_len[1], MPI_CHAR, 0, 203, comm_);
    }
}
#endif

void NetCDFManager::write_timestep_data_to_netcdf(size_t time_index)
{
    if(nc_output_variables_.size() > 0){
        if(nc_output_variables_.size() != data_chunks_.size()){
            LOG("Number of output variables doesn't match the number of data chunks.", LogLevel::FATAL);
            throw std::runtime_error("Mismatch between variable names and the size of the incoming values.");
        }
        for(size_t i = 0; i < nc_output_variables_.size(); ++i)
        {
            const std::string& name = nc_output_variables_[i];
            const std::vector<double>& data = data_chunks_[i];

            if(chunk_count_ == 0) continue; //nothing to write 

            if(data.size() != chunk_count_)
            LOG("Data chunk size mismatch for " + name, LogLevel::FATAL);
                throw std::runtime_error("Data chunk size mismatch for " + name);
            
            auto var = nc_file_->get_ncvar(name);
            if (!var){
                LOG("Catchments variable not found", LogLevel::FATAL);
                throw std::runtime_error("Catchments variable not found");
            }
            var->write_timesliced_data(time_index, chunk_start_, chunk_count_, data.data());
        }
    }
}

std::vector<std::string> NetCDFManager::list_variables() const
{
    if(!nc_file_) return {};
    return nc_file_->list_variables();
}

std::shared_ptr<NetCDFVar> NetCDFManager::get_ncvar_by_name(const std::string& name) const
{
    if(!nc_file_) return nullptr;
    return nc_file_->get_ncvar(name);
}

std::string NetCDFManager::get_string_attribute(const std::string& var_name, const std::string& att_name) const
{
    auto var = nc_file_->get_ncvar(var_name);
    return var->get_string_attribute(att_name);
}

int NetCDFManager::get_int_attribute(const std::string& var_name, const std::string& att_name) const
{
    auto var = nc_file_->get_ncvar(var_name);
    return var->get_int_attribute(att_name);
}

double NetCDFManager::get_double_attribute(const std::string& var_name, const std::string& att_name) const
{
    auto var = nc_file_->get_ncvar(var_name);
    return var->get_double_attribute(att_name);
}

void NetCDFManager::open_file()
{
    if(nc_file_) {
        nc_file_->load_variables();
    } else {
        throw std::runtime_error("NetCDF file not initialized!");
    }
}

void NetCDFManager::close_file()
{
    nc_file_->close_file();
}

// Partition time dimension across ranks
void NetCDFManager::get_local_time_range(size_t& start, size_t& count) const {
#if USE_MPI
    size_t base = num_timesteps_ / size_;
    size_t remainder = num_timesteps_ % size_;
    start = rank_ * base + std::min(static_cast<size_t>(rank_), remainder);
    count = base + (rank_ < static_cast<int>(remainder) ? 1 : 0);
#else
    start = 0;
    count = num_timesteps_;
#endif
}

NetCDFManager::~NetCDFManager() {
    #if NGEN_WITH_MPI
        if(num_procs_ > 1 && comm_ != MPI_COMM_NULL) {
            MPI_Comm_free(&comm_);
        }
    #endif
}
#endif // NGEN_WITH_NETCDF

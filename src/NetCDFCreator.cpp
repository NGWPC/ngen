#include <NGenConfig.h>

#if NGEN_WITH_NETCDF
#include <NetCDFCreator.hpp>
#include <netcdf>
#include <Logger.hpp>

#if NGEN_WITH_MPI
#include <mpi.h>
#include "parallel_utils.h"
#endif

NetCDFCreator::NetCDFCreator(std::shared_ptr<realization::Formulation_Manager> manager, 
    const std::string& output_name,Simulation_Time const& sim_time, int mpi_rank, int mpi_num_procs)
{
    manager_ = manager;
    sim_time_ = std::make_shared<Simulation_Time>(sim_time);
    catchments.reserve(manager_->get_size());
    try{
        
        for (auto const& formulation_info : manager->get_all_formulations())
        {
            std::string catchm = formulation_info.first;
            catchments.push_back(catchm);
        }
        
        #if NGEN_WITH_MPI
            if (mpi_num_procs > 1){
                std::vector<std::string> all_catchments = catchments;
                all_catchments = parallel::gather_strings(catchments, mpi_rank, mpi_num_procs);
                if (mpi_rank == 0){
                    std::sort(all_catchments.begin(), all_catchments.end());
                    all_catchments.erase(
                        std::unique(all_catchments.begin(), all_catchments.end()),
                        all_catchments.end()
                    );
                }
            catchments = parallel::broadcast_strings(all_catchments, mpi_rank, mpi_num_procs);
            }
        #endif
        std::string ncOutputFileName = manager->get_output_root() + output_name + ".nc";
        if(mpi_rank == 0){
            catchmentNcFile = std::make_shared<netCDF::NcFile>(ncOutputFileName, netCDF::NcFile::replace);
            if(!catchmentNcFile){
                LOG("Catchment output netcdf file creation failed: " + ncOutputFileName, LogLevel::FATAL);
                throw std::runtime_error("Catchment output netcdf file creation failed: " + ncOutputFileName);
            }
            //Add dimension and coordinate variable for time
            //TO DO: create a separate function if this action is initiated from outside the class.
            //TO DO: convert seconds epoch to minutes or days?
            int num_timesteps = sim_time_->get_total_output_times();
            auto time_dim = catchmentNcFile->addDim("time", num_timesteps);
            auto time_var = catchmentNcFile->addVar("time", NC_INT, time_dim);
            time_var.putAtt("units", "Seconds since 1970-01-01 00:00:00");
            time_var.putAtt("calendar", "gregorian");
            std::vector<int> time_epoch_seconds(num_timesteps);
            time_epoch_seconds[0] = sim_time_->get_current_epoch_time();
            for(int time_index = 1; time_index < num_timesteps; time_index++)
            {
                sim_time_->advance_timestep();
                time_epoch_seconds[time_index] = sim_time_->get_current_epoch_time();
            }
            time_var.putVar(time_epoch_seconds.data());

            //Add dimension and coordinate variable for catchments
            //TO DO: create a separate function if this action is initiated from outside the class.
            auto catchments_dim = catchmentNcFile->addDim("catchments", catchments.size());
            auto catchments_var = catchmentNcFile->addVar("catchments", NC_STRING, catchments_dim);
            catchments_var.putAtt("Catchment ID", "Catchment identifier in input");
            int item_index = 0;
            std::vector<size_t> index;
            index.resize(1);
            for (auto const& catchment : catchments)
            {
                index[0] = item_index;
                catchments_var.putVar(index,catchment);
                item_index++;
            }

            //Add output data variables information such as headers, variable names, units to netcdf
            //TO DO: change scope of this function if this is initiated from outside the class.
            add_output_variable_info_from_formulation();
            
            //Add global attributes
            catchmentNcFile->putAtt("title", "NextGen Catchment Output Data");
            catchmentNcFile->putAtt("description", "NetCDF file containing catchment-level output data from NextGen simulation");
            catchmentNcFile->putAtt("institution", "NOAA");
        }
        
        #if NGEN_WITH_MPI
            MPI_Barrier(MPI_COMM_WORLD);
        #endif

        if (mpi_rank != 0){
            catchmentNcFile = std::make_shared<netCDF::NcFile>(ncOutputFileName, netCDF::NcFile::write);
            retrieve_output_variables_mpi();
        }
    }
    catch (const netCDF::exceptions::NcException& e){
        LOG(std::string("Error in catchments NetCDF initiation: ") + e.what(), LogLevel::FATAL);
        throw std::runtime_error(std::string("Error in catchments NetCDF initiation: ") + e.what());
    }
}

void NetCDFCreator::add_output_variable_info_from_formulation() 
{
    typename std::map<std::string, std::shared_ptr<realization::Catchment_Formulation>>::const_iterator it = manager_->begin();
    const auto& catchment_info = *it;
    auto r_c = std::dynamic_pointer_cast<realization::Bmi_Formulation>(catchment_info.second);
    if(r_c->get_output_header_count() > 0){
        std::vector<std::string>output_variables = r_c->get_output_variable_names();
        std::vector<std::string>output_headers = r_c->get_output_header_fields();
        std::vector<std::string>output_units = r_c->get_output_variable_units();
        nc_output_variables.resize(output_variables.size());

        std::vector<netCDF::NcDim> dims = {catchmentNcFile->getDim("time"), catchmentNcFile->getDim("catchments")};
        for(int index = 0; index < output_variables.size(); index ++){
            nc_output_variables[index] = catchmentNcFile->addVar(output_headers[index], NC_DOUBLE, dims);
            nc_output_variables[index].putAtt("variable name", output_variables[index]);
            nc_output_variables[index].putAtt("variable units", output_units[index]);
            nc_output_variables[index].putAtt("_FillValue", NC_DOUBLE, -1.0); //TO DO: Change to another value, if recommended.
            nc_output_variables[index].putAtt("missing_value", NC_DOUBLE, -2.0); //TO DO: Change to another value, if recommended.
        }
    }else{
        LOG("No output variables/headers information provided in the realization config. No output variables written to NetCDF.", LogLevel::WARNING);
    }
}

void NetCDFCreator::retrieve_output_variables_mpi() 
{
    typename std::map<std::string, std::shared_ptr<realization::Catchment_Formulation>>::const_iterator it = manager_->begin();
    const auto& catchment_info = *it;
    auto r_c = std::dynamic_pointer_cast<realization::Bmi_Formulation>(catchment_info.second);
    if(r_c->get_output_header_count() > 0){
        std::vector<std::string>output_headers = r_c->get_output_header_fields();
        nc_output_variables.resize(output_headers.size());

        std::vector<netCDF::NcDim> dims = {catchmentNcFile->getDim("time"), catchmentNcFile->getDim("catchments")};
        for(int index = 0; index < output_headers.size(); index ++){
            nc_output_variables[index] = catchmentNcFile->getVar(output_headers[index]);
        }
    }else{
        LOG("No output variables/headers information provided in the realization config. No output variables written to NetCDF.", LogLevel::WARNING);
    }
}

void NetCDFCreator::write_simulations_response_from_formulation(size_t time_index, std::map<std::string, std::string> catchment_output_values)
{
    for (auto const& catchment_val : catchment_output_values)
    {
        std::string catchment_id = catchment_val.first;
        
        //iterate through catchment dimension to find the index of the catchment for writing.
        //also split the comma separated outputs string to a vector of double values
        std::vector<double> catchment_output;
        std::vector<size_t> count = {1, 1};
        for (size_t c_index = 0; c_index < catchments.size(); ++c_index)
        {
            if(catchments[c_index] == catchment_id){
                catchment_output = string_split(catchment_val.second, ',');
                std::vector<size_t> start = {time_index, c_index};
                for(int var_index = 0; var_index < nc_output_variables.size(); ++var_index)
                {
                    nc_output_variables[var_index].putVar(start, count, &catchment_output[var_index]);
                }
                break;
            }
        }
    }
}

std::vector<double> NetCDFCreator::string_split(std::string str, char delimiter)
{
    std::stringstream ss(str);
    std::vector<double> res;
    std::string token;
    while (getline(ss, token, delimiter)) { //will return full string if no comma (or single output variable).
        res.push_back(std::stod(token)); 
    }
    return res;
}

void  NetCDFCreator::close_ncfile(){
    if (catchmentNcFile != nullptr) {
        catchmentNcFile->close();
    }
    catchmentNcFile = nullptr;
}

NetCDFCreator::~NetCDFCreator()
{
    close_ncfile();
}
#endif // NGEN_WITH_NETCDF
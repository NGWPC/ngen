#include <unistd.h>
#include <iostream>
#include <fstream>
#include "realizations/coastal/SchismCreator.h"
#include "realizations/coastal/SchismFormulation.hpp"
#include "forcing/NetCDFMeshPointsDataProvider.hpp"
#include "forcing/MetMeshPolicy.h"
#include "forcing/FlowMeshPolicy.h"
#include "forcing/TidalMeshPolicy.h"

std::unique_ptr<CoastalFormulation>
      SchismCreator::createCoastalFormulation( coastal_config_params const& config,
                                               Simulation_Time const& sim_time ) const
{

      auto param_tree=config.params.get_child("params");

      std::string model_id = param_tree.get<std::string>( "model_type_name" );
      std::string library_file = param_tree.get<std::string>( "library_file");
      std::string met_forcing_file = param_tree.get<std::string>( "met_forcing_netcdf_path" );
      std::string offshore_boundary_file = param_tree.get<std::string>( "offshore_boundary_netcdf_path" );
      std::string flow_boundary_file = param_tree.get<std::string>( "streamflow_boundary_netcdf_path" );
      std::string working_dir = param_tree.get<std::string>( "working_dir" );

      std::string init_config = working_dir + "/namelist.input";

      if ( chdir( working_dir.c_str() ) != 0 ) {
          std::cout << "Fatal: Failed changing current working dir to " << working_dir << std::endl;
      }

      this->writeInitConfig( config, sim_time );

      time_t start_time_t = sim_time.get_start_date_time_epoch();
      time_t stop_time_t = sim_time.get_end_date_time_epoch();

      auto netcdf_met_provider = 
              std::make_shared<data_access::NetCDFMeshPointsDataProvider< 
                                                           data_access::MetMeshPolicy > >(
                      met_forcing_file,
                      std::chrono::system_clock::from_time_t(start_time_t),
                      std::chrono::system_clock::from_time_t(stop_time_t));

      SchismFormulation::check_forcing_provider( *netcdf_met_provider, SchismFormulation::METEO );

      auto netcdf_streamflow_provider = 
              std::make_shared<data_access::NetCDFMeshPointsDataProvider< 
                                                           data_access::FlowMeshPolicy > >(
                      flow_boundary_file,
                      std::chrono::system_clock::from_time_t(start_time_t),
                      std::chrono::system_clock::from_time_t(stop_time_t));

      SchismFormulation::check_forcing_provider( *netcdf_streamflow_provider, 
                                                           SchismFormulation::CHANNEL_FLOW );

      auto netcdf_offshore_provider = 
              std::make_shared<data_access::NetCDFMeshPointsDataProvider< 
                                                           data_access::TidalMeshPolicy > >(
                      offshore_boundary_file,
                      std::chrono::system_clock::from_time_t(start_time_t),
                      std::chrono::system_clock::from_time_t(stop_time_t));

      SchismFormulation::check_forcing_provider( *netcdf_offshore_provider, 
                                                           SchismFormulation::OFFSHORE );
      return std::make_unique<SchismFormulation>( model_id,
                                            library_file,
                                            init_config,
                                            MPI_COMM_SELF,
                                            netcdf_met_provider, 
                                            netcdf_offshore_provider, 
                                            netcdf_streamflow_provider
                                           );
}

SchismCreator* SchismCreator::clone() const
{
      return new SchismCreator();
}

void SchismCreator::writeInitConfig( coastal_config_params const& config,
                                     Simulation_Time const& sim_time ) const
{
      auto param_tree=config.params.get_child("params");

      std::string working_dir = param_tree.get<std::string>( "working_dir" );

      //model_start_time in seconds
      int model_start_time = param_tree.get<int>( "model_start_time_in_secs" );
      int nscribs = param_tree.get<int>( "nscribs" );

      time_t start_time_t = sim_time.get_start_date_time_epoch();

      //create the init config file
      char buffer[100];
      struct tm* timeInfo = gmtime(&start_time_t);
      strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", timeInfo);

      std::string init_config = working_dir + "/namelist.input";

      std::fstream initfile( init_config.c_str(), std::ios::out );
      if (!initfile.is_open()) {
        std::cerr << "Error: Unable to open file!" << std::endl;
        throw std::runtime_error( 
                std::string( "FATAL: Unable to open file - ") + init_config ); 
      }
      
      initfile << "&schism" << std::endl;
      initfile << "model_start_time = " << model_start_time
                << "     !Time start of simulation in seconds" << std::endl;
      initfile << "model_start_year = " << std::string( buffer ).substr(0, 4 ) << std::endl;
      initfile << "model_start_month = " << std::string( buffer ).substr(4, 2 ) << std::endl;
      initfile << "model_start_day = " << std::string( buffer ).substr(6, 2 ) << std::endl;
      initfile << "model_start_hour = " << std::string( buffer ).substr(8, 2 ) << std::endl;
      initfile << "SCHISM_dir       = " << '"' << working_dir << '"' 
              << "   ! SCHISM directory for configuration and forcing files" << std::endl;
      initfile << "SCHISM_NSCRIBES    = " << nscribs 
              <<  "     ! Number of processors to dedicate to I/O methods. "
              "In serial mode, this must be 0 and SCHISM \"OLD IO\" option must be selected "
              "for compiling. If SCHISM BMI is executed in parallel, then this number can "
              "be greater than zero to dedicated and speed up I/O methods. If implemented, "
              "then SCHISM \"OLD IO\" option must be turned off." << std::endl;
      initfile << '/' << std::endl;
      initfile.close();
}

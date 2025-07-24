#include <unistd.h>
#include "realizations/coastal/SchismCreator.h"
#include "realizations/coastal/SchismFormulation.hpp"
#include "realizations/coastal/MockProvider.h"

std::unique_ptr<CoastalFormulation>
      SchismCreator::createCoastalFormulation( coastal_config_params const& config,
		                               Simulation_Time const& sim_time ) const
{

      auto param_tree=config.params.get_child("params");

      std::string model_id = param_tree.get<std::string>( "model_type_name" );
      std::string library_file = param_tree.get<std::string>( "library_file");
      std::string init_config = param_tree.get<std::string>( "init_config" );
      std::string met_forcing_file = param_tree.get<std::string>( "met_forcing_netcdf_path" );
      std::string offshore_boundary_file = param_tree.get<std::string>( "offshore_boundary_netcdf_path" );
      std::string flow_boundary_file = param_tree.get<std::string>( "streamflow_boundary_netcdf_path" );
      std::string working_dir = param_tree.get<std::string>( "working_dir" );
      if ( chdir( working_dir.c_str() ) != 0 ) {
          std::cout << "Fatal: Failed changing current working dir to " << working_dir << std::endl;
      }

      time_t start_time_t = sim_time.get_start_date_time_epoch();
      time_t stop_time_t = sim_time.get_end_date_time_epoch();
      
      auto provider = std::make_shared<MockProvider>();

      return std::make_unique<SchismFormulation>( model_id,
                                            library_file,
					    init_config,
                                            MPI_COMM_SELF,
                                            //netcdf_met_provider,
					    provider, 
                                            provider,
                                            provider
		                           );
}

SchismCreator* SchismCreator::clone() const
{
      return new SchismCreator();
}

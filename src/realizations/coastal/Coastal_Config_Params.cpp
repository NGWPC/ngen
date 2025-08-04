/*
 * Implement the coastal_config_params classs
 */

#include "utilities/logging_utils.h"
#include "realizations/coastal/Coastal_Config_Params.h"

bool coastal_config_params::isValid()
{
	boost::property_tree::ptree::const_assoc_iterator it;
        it = params.find("params");
	if ( it == params.not_found() )
	{
	   logging::critical(std::string("\"params\" not definded in coastal realization!\n").c_str()); 
	   return false;
	}
	boost::property_tree::ptree params_tree = params.get_child( "params" );
	it = params_tree.find("model_type_name");
	if ( it == params_tree.not_found() )
	{
	   logging::critical(std::string("\"model_type_name\" not definded in coastal realization!\n").c_str()); 
	   return false;
	}
	it = params_tree.find("library_file");
	if ( it == params_tree.not_found() )
	{
	   logging::critical(std::string("\"library_file\" not definded in coastal realization!\n").c_str()); 
	   return false;
	}

	it = params_tree.find("model_start_time_in_secs");
	if ( it == params_tree.not_found() )
	{
	   logging::critical(std::string("\"model_start_time_in_secs\" not definded in coastal realization!\n").c_str()); 
	   return false;
	}

	it = params_tree.find("met_forcing_netcdf_path");
	if ( it == params_tree.not_found() )
	{
	   logging::critical(std::string("\"met_forcing_netcdf_path\" not definded in coastal realization!\n").c_str()); 
	   return false;
	}
	it = params_tree.find("offshore_boundary_netcdf_path");
	if ( it == params.not_found() )
	{
	   logging::critical(std::string("\"offshore_boundary_netcdf_path\" not definded in coastal realization!\n").c_str()); 
	   return false;
	}
	it = params_tree.find("offshore_boundary_netcdf_path");
	if ( it == params_tree.not_found() )
	{
	   logging::critical(std::string("\"offshore_boundary_netcdf_path\" not definded in coastal realization!\n").c_str()); 
	   return false;
	}
	it = params_tree.find("offshore_boundary_netcdf_path");
	if ( it == params_tree.not_found() )
	{
	   logging::critical(std::string("\"offshore_boundary_netcdf_path\" not definded in coastal realization!\n").c_str()); 
	   return false;
	}
	it = params_tree.find("streamflow_boundary_netcdf_path");
	if ( it == params_tree.not_found() )
	{
	   logging::critical(std::string("\"streamflow_boundary_netcdf_path\" not definded in coastal realization!\n").c_str()); 
	   return false;
	}
	it = params_tree.find("nscribs");
	if ( it == params_tree.not_found() )
	{
	   logging::critical(std::string("\"nscribs\" not definded in coastal realization!\n").c_str()); 
	   return false;
	}
       return true;
}

ModelType coastal_config_params::getModelType()
{
        std::string model_type = params.get_child("params").get<std::string>("model_type_name" );
        if ( model_type == std::string( "schism_coastal_formulation" ) )
	{
		return ModelType::SCHISM;
	}
	else if ( model_type == std::string("bmi_fortran_sfincs" ) )
	{
		return ModelType::SFINCS;
	}
	else
	{
		logging::critical((std::string("Unknown coastal type: ") + model_type).c_str()); 
                throw std::runtime_error( std::string( "FATAL: Unknown coastal type: ") 
				                         + model_type );
	}  
}

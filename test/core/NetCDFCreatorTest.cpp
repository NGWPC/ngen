#include <NGenConfig.h>


#if NGEN_WITH_NETCDF
#include "gtest/gtest.h"
#include <NetCDFCreator.hpp>
#include <netcdf>
#include "Bmi_Testing_Util.hpp"
#include "FileChecker.h"
#include <features/Features.hpp>
#include <JSONGeometry.hpp>
#include <JSONProperty.hpp>
#include "ewts_ngen/logger.hpp"

class NetCDFCreatorTest : public ::testing::Test {
    protected:
        std::stringstream stream_;
        std::shared_ptr<realization::Formulation_Manager> manager_;
        geojson::GeoJSON fabric = std::make_shared<geojson::FeatureCollection>();
        std::shared_ptr<Simulation_Time> sim_time_;
        std::map<std::string, std::map<long, double>> calculated_results;
        std::unique_ptr<NetCDFCreator> nc_creator;

        typedef struct tm time_type;
        std::shared_ptr<time_type> start_date_time;
        std::shared_ptr<time_type> end_date_time;

        void SetUp() override {
            SetupFormulationManager();
        }

        void TearDown() override {

        }

        //Construct a Formulation Manager and Simulation Time objects
        void SetupFormulationManager() {
            stream_ << fix_paths(example_config);
            boost::property_tree::ptree realization_config;
            boost::property_tree::json_parser::read_json(stream_, realization_config);
            manager_ = std::make_shared<realization::Formulation_Manager>(realization_config);

            auto possible_simulation_time = realization_config.get_child_optional("time");
            if (!possible_simulation_time) {
                std::string throw_msg; throw_msg.assign("ERROR: No simulation time period defined.");
                LOG(throw_msg, LogLevel::WARNING);
                throw std::runtime_error(throw_msg);
            }

            auto simulation_time_config = realization::config::Time(*possible_simulation_time).make_params();
            sim_time_ = std::make_shared<Simulation_Time>(simulation_time_config);

            std::ostream* raw_pointer = &std::cout;
            std::shared_ptr<std::ostream> s_ptr(raw_pointer, [](void*) {});
            utils::StreamHandler catchment_output(s_ptr);

            this->add_feature("cat-52");
            this->add_feature("cat-67");
            manager_->read(simulation_time_config, fabric, catchment_output);
        }

        std::string fix_paths(std::string json)
        {
            std::vector<std::string> forcing_paths = {
                "./data/forcing/cat-52_2015-12-01 00_00_00_2015-12-30 23_00_00.csv",
                "./data/forcing/cat-67_2015-12-01 00_00_00_2015-12-30 23_00_00.csv",
            };
            std::vector<std::string> v = {};
            for(unsigned int i = 0; i < path_options.size(); i++){
                v.push_back( path_options[i] + "data/forcing" );
            }
            std::string dir = utils::FileChecker::find_first_readable(v);
            if(dir != ""){
                std::string remove = "\"./data/forcing/\"";
                std::string replace = "\""+dir+"\"";
                boost::replace_all(json, remove , replace);
            }

            //BMI_CPP_INIT_DIR_PATH
            replace_paths(json, "{{BMI_CPP_INIT_DIR_PATH}}", "data/bmi/test_bmi_cpp");
            //EXTERN_DIR_PATH
            replace_paths(json, "{{EXTERN_LIB_DIR_PATH}}", "extern/test_bmi_cpp/cmake_build/");
            
            for (unsigned int i = 0; i < forcing_paths.size(); i++) {
            if(json.find(forcing_paths[i]) == std::string::npos){
                continue;
            }
            std::vector<std::string> v = {};
            for (unsigned int j = 0; j < path_options.size(); j++) {
                v.push_back(path_options[j] + forcing_paths[i]);
            }
            std::string right_path = utils::FileChecker::find_first_readable(v);
            if(right_path != forcing_paths[i]){
                std::cerr<<"Replacing "<<forcing_paths[i]<<" with "<<right_path<<std::endl;
                boost::replace_all(json, forcing_paths[i] , right_path);
            }
            }
            return json;
        }

        void replace_paths(std::string& input, const std::string& pattern, const std::string& replacement)
        {
            std::vector<std::string> v{path_options.size()};
            for(unsigned int i = 0; i < path_options.size(); i++)
                v[i] = path_options[i] + replacement;
            
            const std::string dir = utils::FileChecker::find_first_readable(v);
            if (dir == "") return;

            boost::replace_all(input, pattern, dir);
        }

        void add_feature(std::string id)
        {
            geojson::three_dimensional_coordinates three_dimensions {
                {
                    {1.0, 2.0},
                    {3.0, 4.0},
                    {5.0, 6.0}
                },
                {
                    {7.0, 8.0},
                    {9.0, 10.0},
                    {11.0, 12.0}
                }
            };
            std::vector<double> bounding_box{1.0, 2.0};
            geojson::PropertyMap properties{};

            geojson::Feature feature = std::make_shared<geojson::PolygonFeature>(geojson::PolygonFeature(
                geojson::polygon(three_dimensions),
                id,
                properties
            ));

            fabric->add_feature(feature);
        }


        std::vector<std::string> path_options = {
                "",
                "../",
                "../../",
                "./test/",
                "../test/",
                "../../test/"

        };

        const std::string example_config =
        "{"
        "    \"global\": {"
        "         \"formulations\": ["
        "             {"
        "                 \"name\": \"bmi_multi\","
        "                 \"params\": {"
        "                     \"model_type_name\": \"bmi_multi_c++\","
        "                     \"forcing_file\": \"\","
        "                     \"init_config\": \"\","
        "                     \"allow_exceed_end_time\": true,"
        "                     \"main_output_variable\": \"OUTPUT_VAR_4\","
        "                     \"uses_forcing_file\": false,"
        "                     \"modules\": ["
        "                         {"
        "                             \"name\": \"bmi_c++\","
        "                             \"params\": {"
        "                                 \"model_type_name\": \"test_bmi_c++\","
        "                                  \"library_file\": \"{{EXTERN_LIB_DIR_PATH}}" BMI_TEST_CPP_LIB_NAME "\","
        "                                  \"init_config\": \"{{BMI_CPP_INIT_DIR_PATH}}/test_bmi_cpp_config_2.txt\","
        "                                 \"allow_exceed_end_time\": true,"
        "                                 \"main_output_variable\": \"OUTPUT_VAR_4\","
        "                                 \"uses_forcing_file\": false,"
        "                                 \"model_params\": {"
        "                                     \"MODEL_VAR_1\": {"
        "                                         \"source\": \"hydrofabric\","
        "                                         \"from\": \"val\""
        "                                     },"
        "                                     \"MODEL_VAR_2\": {"
        "                                         \"source\": \"hydrofabric\""
        "                                     }"
        "                                 },"
        "                                 \"" BMI_REALIZATION_CFG_PARAM_OPT__VAR_STD_NAMES "\": {"
        "                                     \"INPUT_VAR_1\": \"APCP_surface\","
        "                                     \"INPUT_VAR_2\": \"APCP_surface\""
        "                                 }"
        "                             }"
        "                         }"
        "                     ]"
        "                 }"
        "             }"
        "         ],"
        "        \"forcing\": { "
        "            \"file_pattern\": \".*{{id}}.*.csv\","
        "            \"path\": \"./data/forcing/\","
        "            \"provider\": \"CsvPerFeature\""
        "        }"
        "    },"
        "    \"time\": {"
        "        \"start_time\": \"2015-12-01 00:00:00\","
        "        \"end_time\": \"2015-12-30 23:00:00\","
        "        \"output_interval\": 3600"
        "    }"
        "}";
};

TEST_F(NetCDFCreatorTest, TestCatchmentIdentifiers)
{
    nc_creator = std::make_unique<NetCDFCreator>(manager_,"catchment_test", *sim_time_, 0, 1);
    netCDF::NcFile& ncFile = nc_creator->get_ncfile();
    netCDF::NcVar catchments_var = ncFile.getVar("catchments");
    std::vector<netCDF::NcDim> nc_dims = catchments_var.getDims();
    size_t len = nc_dims[0].getSize();
    int item_index = 0;
    std::vector<size_t> index;
    index.resize(1);
    std::vector<std::string> catchments;
    std::string catchment;
    for(size_t i = 0; i < len; ++i){
        index[0] = item_index;
        catchments_var.getVar(index, &catchment);
        catchments.push_back(catchment.c_str());
        item_index++;
    }
    //delete the netcdf file that was created once the information is obtained.
    ncFile.close();
    std::string ncOutputFileName = manager_->get_output_root() + "catchment_test.nc";
    const char* nc_filename = ncOutputFileName.c_str();
    int result = std::remove(nc_filename);
        
    ASSERT_EQ(catchments.size(), 2);
    ASSERT_TRUE(std::find(catchments.begin(), catchments.end(), "cat-52") != catchments.end());
    ASSERT_TRUE(std::find(catchments.begin(), catchments.end(), "cat-67") != catchments.end());
}

TEST_F(NetCDFCreatorTest, TestOutputValues)
{
    nc_creator = std::make_unique<NetCDFCreator>(manager_,"catchment_test", *sim_time_, 0, 1); //create netcdf file or replace this with an existing file?
    
    //write outputs to netcdf at time index 0 for cat-52
    std::map<std::string, std::string> catchment_output_values;
    auto c_form = std::dynamic_pointer_cast<realization::Catchment_Formulation>(manager_->get_formulation("cat-52"));
    double resp = c_form->get_response(0, 3600);
    std::string out_resp = c_form->get_output_line_for_timestep(0);
    catchment_output_values["cat-52"] = out_resp;
    nc_creator->write_simulations_response_from_formulation(0, catchment_output_values);


    auto r_c = std::dynamic_pointer_cast<realization::Bmi_Formulation>(manager_->get_formulation("cat-52"));
    if(r_c->get_output_header_count() > 0){
        std::vector<std::string>output_variables = r_c->get_output_variable_names();
        std::vector<std::string>output_headers = r_c->get_output_header_fields();
        std::vector<std::string>output_units = r_c->get_output_variable_units();

        netCDF::NcFile& ncFile = nc_creator->get_ncfile(); //get a handle to the netcdf file object

        //find the index for "cat-52" and retrieve the values for the output variables
        double catchment_output_nc;
        std::string output_str;
        netCDF::NcVar catchments_var = ncFile.getVar("catchments");
        std::vector<netCDF::NcDim> nc_dims = catchments_var.getDims();
        size_t len = nc_dims[0].getSize();
        size_t catchment_index = 0;
        std::vector<size_t> index;
        index.resize(1);
        std::vector<std::string> catchments;
        std::string catchment;
        for(size_t i = 0; i < len; ++i){
            index[0] = catchment_index;
            catchments_var.getVar(index, &catchment);
            std::string catchm = catchment.c_str();
            if(catchm == "cat-52"){
                //use the catchment_index to query the value from the netcdf file and write it to a string.
                std::vector<size_t> start = {0, catchment_index};
                std::vector<size_t> count = {1, 1};
                for(int var_index = 0; var_index < output_variables.size(); var_index ++){
                    netCDF::NcVar out_var = ncFile.getVar(output_headers[var_index]);
                    out_var.getVar(start, count, &catchment_output_nc);
                    output_str += (output_str.empty() ? "" : ",") + std::to_string(catchment_output_nc);
                }
            }
            catchment_index++;
        }

        //delete the netcdf file that was created once the information is obtained.
        ncFile.close();
        std::string ncOutputFileName = manager_->get_output_root() + "catchment_test.nc";
        const char* nc_filename = ncOutputFileName.c_str();
        int result = std::remove(nc_filename);

        ASSERT_EQ(out_resp, output_str);
    }
}
#endif

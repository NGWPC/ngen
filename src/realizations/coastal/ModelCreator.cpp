/**
 * The ModelCreator class declares the factory method that is supposed to return an
 * object of a CoastalFormulation  class. The ModelCreator's subclasses usually provide the
 * implementation of this method.
 */

#include <cmath>
#include <limits>
#include <iostream>
#include "realizations/coastal/ModelCreator.h"

void ModelCreator::executeModel( coastal_config_params const& config,
                    Simulation_Time const& sim_time ){

            std::unique_ptr<CoastalFormulation> model = 
	      this->createCoastalFormulation( config, sim_time );
            
            model->initialize();

	   // double starttime = model->GetStartTime();
           // std::cout << "starttime = " << starttime << std::endl;

	    double end_time = model->get_end_time();

	    std::cerr << "end_time = " << end_time << std::endl;
	    model->update_until( end_time );
//            for (int i = 0; i < 3; ++i) {
//             std::cout << "Step " << i << std::endl;
//              model->update();
//            }

            using namespace std::chrono_literals;

            auto report = [](std::vector<double> const& data, std::string name) {
               double min = 10e6, max = -10e6;
               for (int i = 0; i < data.size(); ++i) {
                 double val = data[i];
                if (std::isnan(val)) {
                   std::cout << "Nan found at " << i << std::endl;
                   break;
                }

                min = std::min(val, min);
                max = std::max(val, max);
             }
             std::cout << name << " with " << data.size() << " entries ranges from " << min << " to " << max << std::endl;
          };

          MeshPointsSelector bedlevel_selector{"BEDLEVEL", std::chrono::system_clock::now(), 3600s, "m", all_points};
          auto bedlevel = model->get_values(bedlevel_selector, data_access::ReSampleMethod::FRONT_FILL);
          report(bedlevel, "BEDLEVEL");

          for (int i = 0; i < bedlevel.size(); ++i) {
           if (bedlevel[i] == -9999) {
            std::cout << "Bed level is sentinel at index " << i << std::endl;
           }
         }

         MeshPointsSelector eta2_selector{"ETA2", std::chrono::system_clock::now(), 3600s, "m", all_points};
         auto eta2 = model->get_values(eta2_selector, data_access::ReSampleMethod::FRONT_FILL);
         report(eta2, "ETA2");

         MeshPointsSelector tr_eta2_selector{"TROUTE_ETA2", std::chrono::system_clock::now(), 3600s, "m", all_points};
         auto tr_eta2 = model->get_values(tr_eta2_selector, data_access::ReSampleMethod::FRONT_FILL);
         report(tr_eta2, "TROUTE_ETA2");

         MeshPointsSelector vx_selector{"VX", std::chrono::system_clock::now(), 3600s, "m s-1", all_points};
         auto vx = model->get_values(vx_selector, data_access::ReSampleMethod::FRONT_FILL);
         report(vx, "VX");

         MeshPointsSelector vy_selector{"VY", std::chrono::system_clock::now(), 3600s, "m s-1", all_points};
         auto vy = model->get_values(vy_selector, data_access::ReSampleMethod::FRONT_FILL);
         report(vy, "VY");

//         model->update();
         model->get_values(vx_selector, data_access::ReSampleMethod::FRONT_FILL);
         model->get_values(vy_selector, data_access::ReSampleMethod::FRONT_FILL);
         report(vx, "VX");
         report(vy, "VY");

         model->finalize();
    
}


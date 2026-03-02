#ifndef NGENSIMULATION_HPP
#define NGENSIMULATION_HPP

#include <NGenConfig.h>

#include <Simulation_Time.hpp>
#include <Layer.hpp>
#include <Bmi_Adapter.hpp>

namespace hy_features
{
    class HY_Features;
    class HY_Features_MPI;
}

namespace models::bmi
{
    class Bmi_Py_Adapter;
}

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

// Contains all of the dynamic state and logic to run a NextGen hydrologic simulation
class NgenSimulation
{
public:
#if NGEN_WITH_MPI
    using hy_features_t = hy_features::HY_Features_MPI;
#else
    using hy_features_t = hy_features::HY_Features;
#endif

    NgenSimulation(
                   Simulation_Time const& sim_time,
                   std::vector<std::shared_ptr<ngen::Layer>> layers,
                   hy_features_t *features,
                   std::unordered_map<std::string, int> catchment_indexes,
                   std::unordered_map<std::string, int> nexus_indexes,
                   int mpi_rank,
                   int mpi_num_procs
                   );
    NgenSimulation() = delete;

    ~NgenSimulation();

    /**
     * Run the catchment formulations for the full configured duration of the simulation
     *
     * Captures calculated runoff values in `catchment_outflows_` and
     * `nexus_downstream_flows_` for subsequent output and consumption
     * by `run_routing()`
     */
    void run_catchments();

    void initialize_routing(std::string const& t_route_config_file_with_path);
    /**
     * Run t-route on the stored nexus outflow values for the full configured duration of the simulation
     */
    void run_routing();

    int get_nexus_index(std::string const& nexus_id) const;
    double get_nexus_outflow(int nexus_index, int timestep_index) const;

    size_t get_num_output_times() const;
    std::string get_timestamp_for_step(int step) const;

private:
    void advance_models_one_output_step();

    void gather_flows_for_routing(size_t num_steps, boost::span<double> local_downflows, boost::span<double> gathered_downflows);
    void advance_routing_one_step(boost::span<double> downflows);

    int simulation_step_;

    std::shared_ptr<Simulation_Time> sim_time_;

    // Catchment data
    std::vector<std::shared_ptr<ngen::Layer>> layers_;
    hy_features_t *features_;

    // Routing data structured for t-route
    std::unordered_map<std::string, int> catchment_indexes_;
    std::vector<double> catchment_outflows_;
    std::unordered_map<std::string, int> nexus_indexes_;
    std::vector<double> nexus_downstream_flows_;

    std::unique_ptr<models::bmi::Bmi_Adapter> py_troute_ = nullptr;
    size_t global_nexus_count_;
    std::unordered_map<std::string, int> global_nexus_indexes_;
    std::unordered_map<std::string, int> *routing_nexus_indexes_;

    int mpi_rank_;
    int mpi_num_procs_;

    // Serialization template will be defined and instantiated in the .cpp file
    template <class Archive>
    void serialize(Archive& ar);
};

#endif

#pragma once

#include <string>
#include <vector>
#include <memory>

#include <boost/core/span.hpp>

#include "NGenConfig.h"

#include "realizations/coastal/CoastalFormulation.hpp"

// BMI Fortran adapter (correct include + namespace)
#if NGEN_WITH_BMI_FORTRAN
  #include "bmi/Bmi_Fortran_Adapter.hpp"
#endif

/**
 * SfincsFormulation
 *
 * Mirrors the structure of SchismFormulation:
 * - derives from CoastalFormulation (which derives MeshPointsDataProvider)
 * - provides required DataProvider<> pure virtuals
 * - owns a BMI adapter instance
 * - optionally consumes met/offshore/channel boundary providers
 */
class SfincsFormulation final : public CoastalFormulation
{
public:
    // Match SchismFormulation style typedefs
    using ProviderType = data_access::MeshPointsDataProvider;
    using ProviderPtr  = std::shared_ptr<ProviderType>;

    SfincsFormulation(std::string model_id,
                      std::string library_file,
                      std::string init_config,
                      ProviderPtr met_provider,
                      ProviderPtr offshore_provider,
                      ProviderPtr channel_provider);

    ~SfincsFormulation() override;

    // --- BMI lifecycle ---
    void initialize() override;
    void finalize() override;
    void update() override;
    void update_until(double const& t) override;

    // --- Time ---
    double get_current_time() override;
    double get_start_time() override;
    double get_end_time() override;
    double get_time_step() override;

    // --- MeshPointsDataProvider interface (from MeshPointsDataProvider) ---
    // Required by CoastalFormulation (pure virtual)
    void get_values(const selection_type& selector, boost::span<double> data) override;

    // Optional convenience overload (NOT override)
    void get_values(const selection_type& selector, std::vector<double>& out);

    std::size_t mesh_size(const std::string& mesh_name) override;

    // --- DataProvider<double, MeshPointsSelector> pure virtuals ---
    boost::span<const std::string> get_available_variable_names() const override;
    long get_data_start_time() const override;
    long get_data_stop_time() const override;
    long record_duration() const override;
    std::size_t get_ts_index_for_time(const time_t& epoch_time) const override;
    data_type get_value(const selection_type& selector, data_access::ReSampleMethod m=data_access::SUM) override;

private:
    void create_formulation_();
    void destroy_formulation_();

    // Optional: push forcing into BMI vars (keep minimal for now)
    void set_inputs_();

private:
    std::string model_id_;
    std::string library_file_;
    std::string init_config_;

    ProviderPtr met_provider_;
    ProviderPtr offshore_provider_;
    ProviderPtr channel_provider_;

    std::vector<std::string> available_vars_;

#if NGEN_WITH_BMI_FORTRAN
    std::unique_ptr<models::bmi::Bmi_Fortran_Adapter> bmi_;
#endif
};


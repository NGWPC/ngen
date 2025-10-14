#include <NGenConfig.h>

#if NGEN_WITH_BMI_FORTRAN

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <utilities/parallel_utils.h>
#include <realizations/coastal/SfincsFormulation.hpp>

using models::bmi::Bmi_Fortran_Adapter;

static const char* s_sfincs_registration_function = "register_bmi";

// BMI input -> provider mapping (edit as you add inputs)
std::map<std::string, SfincsFormulation::InputMapping>
SfincsFormulation::expected_input_variables_ = {
    {"rain_rate", { SfincsFormulation::METEO, "RAINRATE" }},
};

std::vector<std::string> SfincsFormulation::exported_output_variable_names_ = {
    "TROUTE_ETA2",  // alias of ETA2
    "ETA2",         // zs
    "VX",           // u
    "VY",           // v
    "BEDLEVEL",     // zb
    "DEPTH"         // depth
};

std::map<std::string, std::string> SfincsFormulation::ngen_to_bmi_varname_ = {
    {"TROUTE_ETA2", "zs"},
    {"ETA2",        "zs"},
    {"VX",          "u"},
    {"VY",          "v"},
    {"BEDLEVEL",    "zb"},
    {"DEPTH",       "depth"}
};

SfincsFormulation::SfincsFormulation(std::string const& id,
                                     std::string const& library_path,
                                     std::string const& init_config_path,
                                     std::shared_ptr<ProviderType> met_forcings,
                                     std::shared_ptr<ProviderType> offshore_boundary,
                                     std::shared_ptr<ProviderType> channel_flow_boundary)
: CoastalFormulation(id)
, meteorological_forcings_provider_(std::move(met_forcings))
, offshore_boundary_provider_(std::move(offshore_boundary))
, channel_flow_boundary_provider_(std::move(channel_flow_boundary))
{
    // Non-MPI adapter ctor
    bmi_ = std::make_unique<Bmi_Fortran_Adapter>(
        id, library_path, init_config_path,
        /* model_time_step_fixed = */ true,
        s_sfincs_registration_function
    );
}

SfincsFormulation::~SfincsFormulation() = default;

void SfincsFormulation::initialize()
{
    // ---- Inputs ----
    auto const& input_vars = bmi_->GetInputVarNames();
    for (auto const& bmi_name : input_vars) {
        input_variable_units_[bmi_name]  = bmi_->GetVarUnits(bmi_name);
        input_variable_type_[bmi_name]   = bmi_->GetVarType(bmi_name);
        auto nbytes   = bmi_->GetVarNbytes(bmi_name);
        auto itemsize = bmi_->GetVarItemsize(bmi_name);
        if (itemsize <= 0 || (nbytes % itemsize) != 0) {
            throw std::runtime_error("SFINCS input '" + bmi_name + "': invalid sizes");
        }
        input_variable_count_[bmi_name] = static_cast<size_t>(nbytes / itemsize);
    }

    if (meteorological_forcings_provider_) check_forcing_provider(*meteorological_forcings_provider_);
    if (offshore_boundary_provider_)       check_forcing_provider(*offshore_boundary_provider_);
    if (channel_flow_boundary_provider_)   check_forcing_provider(*channel_flow_boundary_provider_);

    // ---- Outputs ----
    auto const& bmi_outputs = bmi_->GetOutputVarNames();
    for (auto const& ngen_name : exported_output_variable_names_) {
        const std::string bmi_name = to_bmi_name(ngen_name);
        if (std::find(bmi_outputs.begin(), bmi_outputs.end(), bmi_name) == bmi_outputs.end()) {
            throw std::runtime_error("SFINCS BMI missing expected output: " + ngen_name +
                                     " (BMI '" + bmi_name + "')");
        }
        output_variable_units_[ngen_name] = bmi_->GetVarUnits(bmi_name);
        output_variable_type_[ngen_name]  = bmi_->GetVarType(bmi_name);
        auto nbytes   = bmi_->GetVarNbytes(bmi_name);
        auto itemsize = bmi_->GetVarItemsize(bmi_name);
        if (itemsize <= 0 || (nbytes % itemsize) != 0) {
            throw std::runtime_error("SFINCS output '" + ngen_name + "': invalid sizes");
        }
        output_variable_count_[ngen_name] = static_cast<size_t>(nbytes / itemsize);
    }

    // ---- Time ----
    const double ts = bmi_->GetTimeStep();
    if (ts <= 0.0) throw std::runtime_error("SFINCS BMI returned non-positive time step");
    time_step_length_ = std::chrono::seconds(static_cast<long long>(ts));
    current_time_ = std::chrono::system_clock::time_point{
        std::chrono::seconds(static_cast<long long>(bmi_->GetStartTime()))
    };

    set_inputs();
}

void SfincsFormulation::finalize()
{
    if (meteorological_forcings_provider_) meteorological_forcings_provider_->finalize();
    if (offshore_boundary_provider_)       offshore_boundary_provider_->finalize();
    if (channel_flow_boundary_provider_)   channel_flow_boundary_provider_->finalize();
    bmi_->Finalize();
}

void SfincsFormulation::update()
{
    current_time_ += time_step_length_;
    set_inputs();
    bmi_->Update();
}

void SfincsFormulation::update_until(double const& time)
{
    double current = this->get_current_time();
    while (current <= time) {
        set_inputs();
        bmi_->Update();
        current_time_ += time_step_length_;
        current = this->get_current_time();
    }
}

double SfincsFormulation::get_current_time() { return bmi_->GetCurrentTime(); }
double SfincsFormulation::get_start_time()   { return bmi_->GetStartTime(); }
double SfincsFormulation::get_end_time()     { return bmi_->GetEndTime(); }
double SfincsFormulation::get_time_step()    { return bmi_->GetTimeStep(); }

SfincsFormulation::data_type
SfincsFormulation::get_value(const selection_type& selector, data_access::ReSampleMethod)
{
    std::vector<double> tmp(1, 0.0);
    get_values(selector, tmp);
    return tmp.front();
}

void SfincsFormulation::get_values(const selection_type& selector, boost::span<double> data)
{
    const std::string bmi_name = to_bmi_name(selector.variable_name);
    bmi_->GetValue(bmi_name, data.data());
}

size_t SfincsFormulation::mesh_size(std::string const& variable_name)
{
    const std::string bmi_name = to_bmi_name(variable_name);
    auto nbytes   = bmi_->GetVarNbytes(bmi_name);
    auto itemsize = bmi_->GetVarItemsize(bmi_name);
    if (itemsize <= 0 || (nbytes % itemsize) != 0) {
        throw std::runtime_error("SFINCS variable '" + variable_name + "': invalid sizes");
    }
    return static_cast<size_t>(nbytes / itemsize);
}

boost::span<const std::string> SfincsFormulation::get_available_variable_names() const
{
    return exported_output_variable_names_;
}

void SfincsFormulation::set_inputs()
{
    using namespace std::chrono;

    for (auto const& kv : expected_input_variables_) {
        auto const& bmi_name = kv.first;
        auto const& mapping  = kv.second;

        ProviderType* provider = nullptr;
        switch (mapping.provider) {
            case METEO:    provider = meteorological_forcings_provider_.get(); break;
            case OFFSHORE: provider = offshore_boundary_provider_.get();       break;
            case CHANNEL:  provider = channel_flow_boundary_provider_.get();   break;
        }
        if (provider == nullptr) continue;

        std::string units = "";
        auto iu = input_variable_units_.find(bmi_name);
        if (iu != input_variable_units_.end()) units = iu->second;

        AllPoints all;
        MeshPointsSelector points{mapping.provider_name, current_time_, time_step_length_, units, all};

        const size_t n = input_variable_count_[bmi_name];
        std::vector<double> buf(n, 0.0);

        provider->get_values(points, buf);
        bmi_->SetValue(bmi_name, buf.data());
    }
}

// ---------- added: out-of-line definition; currently a no-op ----------
void SfincsFormulation::check_forcing_provider(
    data_access::DataProvider<double, MeshPointsSelector> const& /*provider*/
){
    // No-op for SFINCS today.  Add capability checks here when wiring providers,
    // e.g., units/variable presence, selection support, etc.
}

// Not used by coastal formulation path
long  SfincsFormulation::get_data_start_time() const { throw std::runtime_error(__func__); }
long  SfincsFormulation::get_data_stop_time()  const { throw std::runtime_error(__func__); }
long  SfincsFormulation::record_duration()     const { throw std::runtime_error(__func__); }
size_t SfincsFormulation::get_ts_index_for_time(const time_t&) const { throw std::runtime_error(__func__); }

#endif // NGEN_WITH_BMI_FORTRAN


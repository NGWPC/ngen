#include "realizations/coastal/SfincsFormulation.hpp"

#include <stdexcept>
#include <utility>
#include <limits>

SfincsFormulation::SfincsFormulation(std::string model_id,
                                     std::string library_file,
                                     std::string init_config,
                                     ProviderPtr met_provider,
                                     ProviderPtr offshore_provider,
                                     ProviderPtr channel_provider)
    : CoastalFormulation(model_id) // IMPORTANT: CoastalFormulation requires id
    , model_id_(std::move(model_id))
    , library_file_(std::move(library_file))
    , init_config_(std::move(init_config))
    , met_provider_(std::move(met_provider))
    , offshore_provider_(std::move(offshore_provider))
    , channel_provider_(std::move(channel_provider))
{
}

SfincsFormulation::~SfincsFormulation()
{
    // be safe if finalize wasn't called
    try { finalize(); } catch (...) {}
}

static std::string normalize_var(const std::string& v)
{
    if (v == "BEDLEVEL" || v == "bedlevel" || v == "bed_level")
        return "zb";
    return v;
}

void SfincsFormulation::create_formulation_()
{
#if NGEN_WITH_BMI_FORTRAN
    if (bmi_) return;

    // Bmi_Fortran_Adapter constructors (per your compile error):
    //  - (type_name, library, has_fixed_dt, reg_func)
    //  - (type_name, library, init_config, has_fixed_dt, reg_func)
    //
    // We want to pass init_config_ so use the 5-arg overload.
    const bool has_fixed_time_step = false;

    // Default in adapter header is "register_bmi", but pass explicitly for clarity.
    const std::string registration_function = "register_bmi";

    bmi_ = std::make_unique<models::bmi::Bmi_Fortran_Adapter>(
        model_id_,          // type_name
        library_file_,      // library_file_path
        init_config_,       // bmi_init_config
        has_fixed_time_step,
        registration_function
    );
#else
    throw std::runtime_error("SfincsFormulation requires NGEN_WITH_BMI_FORTRAN=ON");
#endif
}

void SfincsFormulation::destroy_formulation_()
{
#if NGEN_WITH_BMI_FORTRAN
    bmi_.reset();
#endif
    available_vars_.clear();
}

void SfincsFormulation::initialize()
{
    create_formulation_();

#if NGEN_WITH_BMI_FORTRAN
    bmi_->Initialize();

    available_vars_.clear();
#endif
}

void SfincsFormulation::finalize()
{
#if NGEN_WITH_BMI_FORTRAN
    if (bmi_) {
        bmi_->Finalize();
    }
#endif
    destroy_formulation_();
}

void SfincsFormulation::update()
{
#if NGEN_WITH_BMI_FORTRAN
    if (!bmi_) {
        throw std::runtime_error("SfincsFormulation::update called before initialize()");
    }

    // (Optional) push forcings into BMI variables
    // set_inputs_();

    bmi_->Update();
#else
    throw std::runtime_error("SfincsFormulation requires NGEN_WITH_BMI_FORTRAN=ON");
#endif
}

void SfincsFormulation::update_until(double const& t)
{
#if NGEN_WITH_BMI_FORTRAN
    if (!bmi_) {
        throw std::runtime_error("SfincsFormulation::update_until called before initialize()");
    }

    // Mirror Schism behavior
    while (bmi_->GetCurrentTime() < t) {
        // set_inputs_();
        bmi_->Update();
    }
#else
    (void)t;
    throw std::runtime_error("SfincsFormulation requires NGEN_WITH_BMI_FORTRAN=ON");
#endif
}

double SfincsFormulation::get_current_time()
{
#if NGEN_WITH_BMI_FORTRAN
    return bmi_ ? bmi_->GetCurrentTime() : 0.0;
#else
    return 0.0;
#endif
}

double SfincsFormulation::get_start_time()
{
#if NGEN_WITH_BMI_FORTRAN
    return bmi_ ? bmi_->GetStartTime() : 0.0;
#else
    return 0.0;
#endif
}

double SfincsFormulation::get_end_time()
{
#if NGEN_WITH_BMI_FORTRAN
    return bmi_ ? bmi_->GetEndTime() : 0.0;
#else
    return 0.0;
#endif
}

double SfincsFormulation::get_time_step()
{
#if NGEN_WITH_BMI_FORTRAN
    return bmi_ ? bmi_->GetTimeStep() : 0.0;
#else
    return 0.0;
#endif
}

void SfincsFormulation::get_values(const selection_type& selector, boost::span<double> out)
{
    const std::string var = normalize_var(selector.variable_name);

#if NGEN_WITH_BMI_FORTRAN
    if (!bmi_) {
        throw std::runtime_error("SfincsFormulation::get_values called before initialize()");
    }

    if (out.empty()) {
        return;
    }

    bmi_->GetValue(var, out.data());
#else
    (void)var;
    std::fill(out.begin(), out.end(), 0.0);
#endif
}

void SfincsFormulation::get_values(const selection_type& selector, std::vector<double>& out)
{
#if NGEN_WITH_BMI_FORTRAN
    if (!bmi_) {
        throw std::runtime_error("SfincsFormulation::get_values called before initialize()");
    }

    const std::string& var = selector.variable_name;

    if (out.empty()) {
        const auto nbytes   = bmi_->GetVarNbytes(var);
        const auto itemsize = bmi_->GetVarItemsize(var);
        if (itemsize == 0) {
            throw std::runtime_error("BMI reported itemsize=0 for var: " + var);
        }
        out.resize(static_cast<std::size_t>(nbytes / itemsize));
    }

    get_values(selector, boost::span<double>(out.data(), out.size()));
#else
    std::fill(out.begin(), out.end(), 0.0);
#endif
}

std::size_t SfincsFormulation::mesh_size(const std::string& mesh_name)
{
#if NGEN_WITH_BMI_FORTRAN
    if (!bmi_) return 0;

    const auto nbytes   = bmi_->GetVarNbytes(mesh_name);
    const auto itemsize = bmi_->GetVarItemsize(mesh_name);
    if (itemsize == 0) return 0;

    return static_cast<std::size_t>(nbytes / itemsize);
#else
    (void)mesh_name;
    return 0;
#endif
}

// ---------------------
// DataProvider<> required pure virtuals
// ---------------------

boost::span<const std::string> SfincsFormulation::get_available_variable_names() const
{
    return boost::span<const std::string>(available_vars_.data(), available_vars_.size());
}

long SfincsFormulation::get_data_start_time() const
{
    // As a formulation, this isn’t a forcing provider; return model start epoch seconds if available.
    // If you want exact forcing provider time, return met_provider_->get_data_start_time().
    return 0;
}

long SfincsFormulation::get_data_stop_time() const
{
    return 0;
}

long SfincsFormulation::record_duration() const
{
    // duration in seconds between forcing records if acting as provider; unknown here
    return 0;
}

std::size_t SfincsFormulation::get_ts_index_for_time(const time_t& /*epoch_time*/) const
{
    return 0;
}

SfincsFormulation::data_type
SfincsFormulation::get_value(const selection_type& selector, data_access::ReSampleMethod /*m*/)
{
    // Provide a scalar fetch convenience via get_values
    std::vector<double> buf;
    buf.reserve(1);
    get_values(selector, buf);
    if (buf.empty()) return 0.0;
    return buf[0];
}

// Optional: later we can wire forcings into BMI inputs similar to Schism’s set_inputs()
// For now keep it a no-op so compilation is stable.
void SfincsFormulation::set_inputs_()
{
    // Intentionally minimal.
    // Once you confirm SFINCS BMI input variable names, we can map met/offshore/channel providers.
}


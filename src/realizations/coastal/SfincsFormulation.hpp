#pragma once

#include <memory>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <utility>
#include <cmath>

#include <boost/property_tree/ptree.hpp>
#include <boost/optional.hpp>

// ---- Adjust these includes/namespaces to your ngen branch ----
// If your coastal formulations derive from a coastal-specific base,
// change the include + base class alias below accordingly.
#include "Formulation.hpp"                  // generic ngen base (catchment/coastal)
#include "JSONProperty.hpp"                 // helpers for ptree access if available
#include "util/Date_Time_Utilities.hpp"     // for seconds, time conversions if needed

// If your branch has a distinct coastal base class, alias it here:
namespace realization = ngen::realization;  // adjust if your namespace differs

namespace ngen {
namespace realization {
namespace coastal {

/**
 * @brief BMI-backed SFINCS formulation for NGen.
 *
 * This class wraps a BMI 2.0 implementation of SFINCS (your Fortran `bmif_2_0`
 * module compiled as a shared lib with C bindings or linked directly through
 * an adapter), exposing the standard NGen formulation interface.
 *
 * Configuration (PTree keys expected):
 *   - "library"        : Path to the SFINCS BMI shared library (if using dlopen)
 *   - "config_file"    : Path to SFINCS run/config file (e.g., "sfincs_config.txt")
 *   - "work_dir"       : (optional) Working directory for the model
 *   - "time_step"      : (optional) seconds (if you want to override BMI’s dt)
 *   - "outputs"        : (optional) array of variable names to publish
 *
 * Exposed variables (default):
 *   - "z", "h", "un", "vn"
 * You can expand or rename these in `init_var_maps_()`.
 */
class SfincsFormulation : public realization::Formulation
{
public:
    using ptree = boost::property_tree::ptree;

    SfincsFormulation() = default;
    ~SfincsFormulation() override { finalize_noexcept_(); }

    // ---- lifecycle -----------------------------------------------------

    /**
     * Configure, load, and initialize the BMI model.
     */
    void create_formulation(
        const std::string& forcing_file_path,  // unused; kept for interface symmetry
        utils::StreamHandler output_stream,
        const ptree& config_tree,
        const std::string& registration_name,
        bool ignore_output_root = false
    ) override
    {
        (void)forcing_file_path;
        (void)output_stream;
        (void)registration_name;
        (void)ignore_output_root;

        config_ = config_tree;

        // Required config
        config_file_ = get_required_(config_, "config_file");
        // Optional (if you use dlopen + C-BMI). If your build links Fortran directly,
        // you can ignore "library".
        library_path_ = config_get_(config_, "library", std::string(""));
        work_dir_     = config_get_(config_, "work_dir", std::string(""));

        // Optional override of dt (seconds)
        dt_override_s_ = config_get_optional<double>(config_, "time_step");

        // Choose outputs (if omitted, we’ll publish defaults)
        if (auto op = config_.get_child_optional("outputs")) {
            for (auto& kv : *op) outputs_.push_back(kv.second.get_value<std::string>());
        }

        // Initialize adapter (dlopen or direct). See adapter_ below.
        init_bmi_adapter_();

        // Initialize variable maps and buffers
        init_var_maps_();
        alloc_state_buffers_();

        // Pull times from BMI
        double start{}, current{}, end{}, dt{};
        chk_(adapter_->get_start_time(start), "get_start_time");
        chk_(adapter_->get_current_time(current), "get_current_time");
        chk_(adapter_->get_end_time(end), "get_end_time");
        chk_(adapter_->get_time_step(dt), "get_time_step");

        start_time_s_   = start;
        current_time_s_ = current;
        end_time_s_     = end;
        time_step_s_    = dt_override_s_.value_or(dt);

        // If no explicit outputs given, pick defaults
        if (outputs_.empty())
            outputs_ = {"z", "h", "un", "vn"};
    }

    /**
     * Advance by one time step.
     */
    void update() override
    {
        chk_(adapter_->update(), "update");
        double t{};
        chk_(adapter_->get_current_time(t), "get_current_time");
        current_time_s_ = t;
    }

    /**
     * Advance until absolute time (seconds since model start; match BMI).
     */
    void update_until(double t_abs_s)
    {
        chk_(adapter_->update_until(t_abs_s), "update_until");
        current_time_s_ = t_abs_s;
    }

    void finalize() override { finalize_noexcept_(); }

    // ---- query & response ----------------------------------------------

    /**
     * Return a named scalar (or 1-element) output as a string (NGen legacy).
     * For coastal/array data, you’ll typically use get_value() below.
     */
    std::string get_response()
    {
        // Example: return water level "h" as string (first elem)
        // If you prefer another default, change here or wire through config.
        ensure_var_cached_("h");
        if (state_r_["h"].empty()) return "nan";
        return std::to_string(state_r_["h"][0]);
    }

    /**
     * Get an array output by name into a provided vector<double>.
     * (Convenience utility for formulations that need bulk values.)
     */
    void get_value(const std::string& name, std::vector<double>& dest)
    {
        ensure_var_cached_(name);
        auto it = state_r_.find(name);
        if (it != state_r_.end()) {
            dest = it->second;
            return;
        }
        // Try integer state (promote to double)
        auto itI = state_i_.find(name);
        if (itI != state_i_.end()) {
            dest.resize(itI->second.size());
            for (std::size_t k = 0; k < dest.size(); ++k) dest[k] = static_cast<double>(itI->second[k]);
            return;
        }
        throw std::runtime_error("Unknown variable in get_value: " + name);
    }

    /**
     * Return the set of outputs this formulation will publish.
     */
    std::vector<std::string> get_available_output_names() const override { return outputs_; }

    /**
     * Return the current model time (seconds; BMI units).
     */
    double get_current_time() const noexcept { return current_time_s_; }

    double get_end_time() const noexcept { return end_time_s_; }
    double get_time_step() const noexcept { return time_step_s_; }
    double get_start_time() const noexcept { return start_time_s_; }

    /**
     * Basic identifier (useful in logs).
     */
    std::string get_formulation_identifier() const override { return "sfincs-bmi"; }

private:
    // ---- minimal BMI adapter facade -----------------------------------
    //
    // You likely already have a C/C++ BMI adapter in your branch. If so,
    // replace this with your project’s adapter type and construction.
    //
    // The interface expected here is:
    //   int initialize(const char*);
    //   int update();
    //   int update_until(double);
    //   int finalize();
    //   int get_var_grid(const char*, int*);
    //   int get_grid_size(int, int*);
    //   int get_value(const char*, double* or int*);
    //   int set_value(const char*, const double* or int*);
    //   int get_start_time(double*), get_current_time(double*),
    //       get_end_time(double*), get_time_step(double*);
    //   int get_component_name(char*), get_time_units(char*), etc.
    //
    struct BmiAdapter
    {
        virtual ~BmiAdapter() = default;

        virtual int initialize(const std::string& config_file) = 0;
        virtual int update() = 0;
        virtual int update_until(double t) = 0;
        virtual int finalize() = 0;

        virtual int get_start_time(double& t) = 0;
        virtual int get_current_time(double& t) = 0;
        virtual int get_end_time(double& t) = 0;
        virtual int get_time_step(double& dt) = 0;

        virtual int get_var_grid(const std::string& name, int& grid) = 0;
        virtual int get_grid_size(int grid, int& n) = 0;

        virtual int get_value(const std::string& name, std::vector<int>& dest) = 0;
        virtual int get_value(const std::string& name, std::vector<float>& dest) = 0;
        virtual int get_value(const std::string& name, std::vector<double>& dest) = 0;

        virtual int set_value(const std::string& name, const std::vector<int>& src) = 0;
        virtual int set_value(const std::string& name, const std::vector<float>& src) = 0;
        virtual int set_value(const std::string& name, const std::vector<double>& src) = 0;
    };

    // ---- concrete adapter selection -----------------------------------
    void init_bmi_adapter_()
    {
        // If your NGen branch already has a generic BMI adapter (e.g., dlopen-based),
        // construct that here instead. For now, assume a linked adapter is available:
        adapter_ = make_linked_adapter_();   // replace with your factory as needed

        const int s = adapter_->initialize(config_file_);
        chk_(s, "initialize");
    }

    // Placeholder construction; replace with your real adapter type.
    std::shared_ptr<BmiAdapter> make_linked_adapter_();

    // ---- variable discovery & state -----------------------------------
    void init_var_maps_()
    {
        // Map BMI variable names -> our published names.
        // If your BMI uses different names, adjust here.
        // SFINCS (examples):
        //   - free-surface elevation:  h
        //   - bed elevation:           z
        //   - depth-avg velocities:    un, vn
        bmi_to_pub_ = {
            {"z",  "z"},
            {"h",  "h"},
            {"un", "un"},
            {"vn", "vn"}
        };
    }

    void alloc_state_buffers_()
    {
        // Allocate arrays based on each var’s grid size.
        for (auto& kv : bmi_to_pub_) {
            const std::string& bmi_name = kv.first;
            int grid = 0, n = 0;
            chk_(adapter_->get_var_grid(bmi_name, grid), "get_var_grid(" + bmi_name + ")");
            chk_(adapter_->get_grid_size(grid, n),       "get_grid_size(" + bmi_name + ")");
            // Assume real(8) for z/h and real(4) for un/vn unless overridden:
            if (bmi_name == "z" || bmi_name == "h") {
                state_r_[kv.second].assign(n, std::nan(""));
            }
            else if (bmi_name == "un" || bmi_name == "vn") {
                state_r_[kv.second].assign(n, 0.0); // double buffer, even if BMI provides float
            }
            else {
                state_r_[kv.second].assign(n, std::nan(""));
            }
        }
    }

    void ensure_var_cached_(const std::string& pub_name)
    {
        // Find BMI name corresponding to pub_name
        std::string bmi_name = pub_to_bmi_(pub_name);

        // Pull values from BMI into our double buffers.
        // Try highest precision first, then fallbacks.
        {
            std::vector<double> tmp;
            if (adapter_->get_value(bmi_name, tmp) == 0) {  // BMI_SUCCESS == 0
                state_r_[pub_name] = std::move(tmp);
                return;
            }
        }
        {
            std::vector<float> tmp;
            if (adapter_->get_value(bmi_name, tmp) == 0) {
                auto& d = state_r_[pub_name];
                d.resize(tmp.size());
                for (std::size_t i = 0; i < tmp.size(); ++i) d[i] = static_cast<double>(tmp[i]);
                return;
            }
        }
        {
            std::vector<int> tmp;
            if (adapter_->get_value(bmi_name, tmp) == 0) {
                auto& d = state_r_[pub_name];
                d.resize(tmp.size());
                for (std::size_t i = 0; i < tmp.size(); ++i) d[i] = static_cast<double>(tmp[i]);
                return;
            }
        }
        throw std::runtime_error("Failed to fetch variable via BMI: " + pub_name + " (" + bmi_name + ")");
    }

    std::string pub_to_bmi_(const std::string& pub) const
    {
        for (auto& kv : bmi_to_pub_) if (kv.second == pub) return kv.first;
        // If direct match, allow passthrough
        return pub;
    }

    // ---- utils ---------------------------------------------------------
    template <typename T>
    static T config_get_(const ptree& p, const std::string& key, const T& deflt) {
        return p.get<T>(key, deflt);
    }
    static std::string get_required_(const ptree& p, const std::string& key) {
        auto v = p.get<std::string>(key, "");
        if (v.empty()) throw std::runtime_error("Missing required config key: " + key);
        return v;
    }
    template <typename T>
    static boost::optional<T> config_get_optional(const ptree& p, const std::string& key) {
        if (auto o = p.get_optional<T>(key)) return o;
        return {};
    }

    static void chk_(int s, const char* where) {
        if (s != 0) throw std::runtime_error(std::string("BMI call failed: ") + where);
    }
    static void chk_(int s, const std::string& where) {
        chk_(s, where.c_str());
    }

    void finalize_noexcept_() noexcept {
        if (adapter_) { (void)adapter_->finalize(); adapter_.reset(); }
    }

private:
    // Config & model paths
    ptree        config_;
    std::string  config_file_;
    std::string  library_path_;
    std::string  work_dir_;
    boost::optional<double> dt_override_s_;

    // BMI adapter
    std::shared_ptr<BmiAdapter> adapter_;

    // Time bookkeeping (seconds in BMI’s units)
    double start_time_s_   = 0.0;
    double current_time_s_ = 0.0;
    double end_time_s_     = 0.0;
    double time_step_s_    = 0.0;

    // Outputs to publish
    std::vector<std::string> outputs_;

    // Name maps and cached states
    std::unordered_map<std::string, std::string> bmi_to_pub_; // bmi_name -> published_name
    std::unordered_map<std::string, std::vector<double>> state_r_;
    std::unordered_map<std::string, std::vector<int>>    state_i_;
};

} // namespace coastal
} // namespace realization
} // namespace ngen


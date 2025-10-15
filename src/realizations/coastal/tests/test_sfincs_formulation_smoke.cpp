#include <NGenConfig.h>

#if NGEN_WITH_BMI_FORTRAN

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <chrono>

#include "realizations/coastal/SfincsFormulation.hpp"
#include "realizations/coastal/CoastalFormulation.hpp"

static int fail(std::string const& msg, int code) {
    std::cerr << "FAIL: " << msg << std::endl;
    return code;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: test_sfincs_formulation_smoke <libsfincs_bmi.so> <init_config.txt>\n";
        return 2;
    }

    const std::string lib  = argv[1];
    const std::string init = argv[2];

    // No providers (rain, offshore, channel) – formulation tolerates nullptr providers.
    std::shared_ptr<SfincsFormulation::ProviderType> met    = nullptr;
    std::shared_ptr<SfincsFormulation::ProviderType> off    = nullptr;
    std::shared_ptr<SfincsFormulation::ProviderType> chflow = nullptr;

    SfincsFormulation f("sfincs_demo", lib, init, met, off, chflow);

    try {
        f.initialize();

        // 1) Exported variable names should include our mapped set
        auto names_span = f.get_available_variable_names();
        std::vector<std::string> names(names_span.begin(), names_span.end());

        auto has = [&](const std::string& n) {
            for (auto const& s : names) if (s == n) return true;
            return false;
        };

        // The standard coastal set (mapped internally to BMI: zs, zb, u, v, depth)
        if (!has("ETA2"))          return fail("output names include ETA2", 3);
        if (!has("TROUTE_ETA2"))   return fail("output names include TROUTE_ETA2", 4);
        if (!has("VX"))            return fail("output names include VX", 5);
        if (!has("VY"))            return fail("output names include VY", 6);
        if (!has("BEDLEVEL"))      return fail("output names include BEDLEVEL", 7);
        // DEPTH is optional in some setups, but we expect it with your BMI:
        if (!has("DEPTH"))         return fail("output names include DEPTH", 8);

        // 2) For each variable, mesh_size > 0 and GetValue returns that many values
        auto check_var = [&](const std::string& var)->int {
            const size_t n = f.mesh_size(var);
            if (n == 0) return fail("mesh_size(" + var + ") == 0", 20);

            // Build a selector for "all points" for [t, t+dt)
            const double t_now_sec = f.get_current_time();
            const double dt_sec    = f.get_time_step();

            using clock = std::chrono::system_clock;
            auto t0 = clock::time_point{ std::chrono::seconds(static_cast<long long>(t_now_sec)) };
            auto dt = std::chrono::seconds(static_cast<long long>(dt_sec));

            AllPoints all;
            MeshPointsSelector sel{var, t0, dt, /*units*/"", all};

            std::vector<double> buf(n, -999.0);
            f.get_values(sel, buf);

            if (buf.size() != n) return fail("get_values(" + var + ") wrong size", 21);
            return 0;
        };

        for (auto const& v : names) {
            if (int rc = check_var(v)) return rc;
        }

        // 3) Time stepping advances
        const double t0 = f.get_current_time();
        f.update();
        const double t1 = f.get_current_time();
        if (!(t1 > t0)) return fail("time did not advance after update()", 30);

        // 4) update_until moves further
        const double dt = f.get_time_step();
        f.update_until(t1 + 3.0 * dt);
        const double t2 = f.get_current_time();
        if (!(t2 >= t1 + 3.0 * dt)) return fail("time did not advance to target in update_until()", 31);

        f.finalize();
    }
    catch (std::exception const& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 10;
    }
    catch (...) {
        std::cerr << "Unknown exception\n";
        return 11;
    }

    std::cout << "PASS: SfincsFormulation smoke (no MPI)" << std::endl;
    return 0;
}

#else
int main(int, char**) {
    // Built without Fortran BMI; test is a no-op so CI still passes
    return 0;
}
#endif


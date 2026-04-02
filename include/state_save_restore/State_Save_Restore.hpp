#ifndef NGEN_STATE_SAVE_RESTORE_HPP
#define NGEN_STATE_SAVE_RESTORE_HPP

#include <NGenConfig.h>

#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/core/span.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "vecbuf.hpp"
#include "State_Save_Utils.hpp"

class State_Saver;
class State_Loader;
class State_Snapshot_Saver;

class State_Save_Config
{
public:
    /**
     * Expects the tree @param config that potentially contains a "state_saving" key
     *
     *
     */
    State_Save_Config(boost::property_tree::ptree const& config);

    /**
     * Get state loaders that perform before the catchments are run.
     * 
     * @return `std::pair`s of the label from the config and an instance of the loader.
     */
    std::vector<std::pair<std::string, std::shared_ptr<State_Loader>>> start_of_run_loaders() const;

    /**
     * Get state savers that perform after the catchments have run to completion.
     * 
     * @return `std::pair`s of the label from the config and an instance of the saver.
     */
    std::vector<std::pair<std::string, std::shared_ptr<State_Saver>>> end_of_run_savers() const;

    /**
     * Get state loader that is intended to be performed before catchment processing starts.
     * 
     * The returned pointer may be NULL if no configuration was made for existing data.
     */
    std::unique_ptr<State_Loader> hot_start() const;

    std::unique_ptr<State_Loader> checkpoint_loader() const;

    bool has_checkpoint_saver() const;

    std::shared_ptr<State_Saver> checkpoint_saver(int *const frequency) const;

    struct instance
    {
        instance(std::string const& direction, std::string const& label, std::string const& path, std::string const& mechanism, std::string const& timing, boost::optional<int> frequency);

        State_Save_Direction direction_;
        State_Save_Mechanism mechanism_;
        State_Save_When timing_;
        std::string label_;
        std::string path_;
        int frequency_;

        std::string mechanism_string() const;
    };

private:
    std::vector<instance> instances_;
};

class State_Saver
{
public:
    // Flag type to indicate whether state saving needs to ensure
    // stability of saved data wherever it is stored before returning
    // success
    enum class State_Durability { relaxed, strict };

    State_Saver() = default;
    virtual ~State_Saver() = default;

    /**
     * Return an object suitable for saving a simulation state as of a
     * particular moment in time, @param epoch
     *
     * @param durability indicates whether callers expect all
     * potential errors to be checked and reported before finalize()
     * and/or State_Snapshot_Saver::finish_saving() return
     */
    virtual std::shared_ptr<State_Snapshot_Saver> initialize_snapshot(State_Durability durability) = 0;

    virtual std::shared_ptr<State_Snapshot_Saver> initialize_checkpoint_snapshot(int step, State_Durability durability) = 0;

    /** Clear unneeded data that may be generated over the lifetime of a State_Saver's lifetime
     * 
     * @param mpi_rank The process' MPI rank that may be used for determining responsibility of cleanup.
     */
    virtual void clear_cache(int mpi_rank) = 0;

    /**
     * Execute any logic necessary to cleanly finish usage, and
     * potentially report errors, before destructors would
     * execute. E.g. closing files opened in parallel across MPI
     * ranks.
     */
    virtual void finalize() = 0;
};

class State_Snapshot_Saver
{
public:
    State_Snapshot_Saver() = delete;
    State_Snapshot_Saver(State_Saver::State_Durability durability);
    virtual ~State_Snapshot_Saver() = default;

    /**
     * Capture the data from a single unit of the simulation
     */
    virtual void save_unit(std::string const& unit_name, boost::span<char const> data) = 0;

    /**
     * Capture the data from a single unit that can be serialized with a Boost binary_oarchive
     */
    template <class T>
    void archive_unit(const std::string &unit_name, T *item) {
        vecbuf<char> buffer;
        boost::archive::binary_oarchive archive(buffer);
        archive << (*item);
        boost::span<char> data(buffer.data(), buffer.size());
        this->save_unit(unit_name, data);
    }

    /**
     * Execute logic to complete the saving process
     *
     * Data may be flushed here, and delayed errors may be detected
     * and reported here. With relaxed durability, error reports may
     * not come until the parent State_Saver::finalize() call is made,
     * or ever.
     */
    virtual void finish_saving() = 0;

protected:
    State_Saver::State_Durability durability_;
};

class State_Snapshot_Loader;

class State_Loader
{
public:
    State_Loader() = default;
    virtual ~State_Loader() = default;

    /**
     * Return an object suitable for loading a simulation state as of
     * a particular moment in time, @param epoch
     */
    virtual std::shared_ptr<State_Snapshot_Loader> initialize_snapshot() = 0;

    /**
     * Return an object suitable for loading a checkpoint simulation state with all the required units.
     * 
     * @param required_units Vector of all units that are required for a simulation state to be considered complete.
     * @param checkpoint_step Pointer that will be set to the step number of the valid state.
     */
    virtual std::shared_ptr<State_Snapshot_Loader> initialize_checkpoint_snapshot(const std::vector<std::string> &required_units) = 0;

    /**
     * Execute any logic necessary to cleanly finish usage, and
     * potentially report errors, before destructors would
     * execute. E.g. closing files opened in parallel across MPI
     * ranks.
     */
    virtual void finalize() = 0;
};

class State_Snapshot_Loader
{
public:
    State_Snapshot_Loader() = default;
    virtual ~State_Snapshot_Loader() = default;

    /**
     * Check if data of a unit name exists.
     */
    virtual bool has_unit(const std::string &unit_name) = 0;

    /**
     * Load data from whatever source, and pass it to @param unit_loader->load()
     */
    virtual void load_unit(const std::string &unit_name, std::vector<char> &data) = 0;

    /**
     * Load unit data and immediately dearchive it with a Boost binary_iarchive
     */
    template<class T>
    void dearchive_unit(const std::string &unit_name, T *item) {
        std::vector<char> buffer;
        this->load_unit(unit_name, buffer);
        membuf data(buffer.data(), buffer.size());
        boost::archive::binary_iarchive archive(data);
        archive >> (*item);
    }

    /**
     * Execute logic to complete the saving process
     *
     * Data may be flushed here, and delayed errors may be detected
     * and reported here. With relaxed durability, error reports may
     * not come until the parent State_Saver::finalize() call is made,
     * or ever.
     */
    virtual void finish_saving() = 0;
};

#endif // NGEN_STATE_SAVE_RESTORE_HPP

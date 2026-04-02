#include <state_save_restore/File_Per_Unit.hpp>
#include "Logger.hpp"

#if __has_include(<filesystem>) && __cpp_lib_filesystem >= 201703L
  #include <filesystem>
  using namespace std::filesystem;
  #warning "Using STD Filesystem"
#elif __has_include(<experimental/filesystem>) && defined(__cpp_lib_filesystem)
  #include <experimental/filesystem>
  using namespace std::experimental::filesystem;
  #warning "Using Filesystem TS"
#elif __has_include(<boost/filesystem.hpp>)
  #include <boost/filesystem.hpp>
  using namespace boost::filesystem;
  #warning "Using Boost.Filesystem"
#else
  #error "No Filesystem library implementation available"
#endif

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>

#include <fstream>
#include <iomanip>

namespace {
    // Populate a vector of paths with subfolders with names that can be interpreted as ints.
    // The vector will be sorted by highest numeric representation first.
    void ordered_checkpoint_subfolders(const std::string &root, std::vector<path> &subdirs) {
        for (const auto &subdir : directory_iterator(root)) {
            path subdir_path = subdir.path();
            // make sure subfolder is a number from a timestep
            if (subdir_path.filename().string().find_first_not_of("0123456789") == std::string::npos) {
                subdirs.push_back(subdir);
            }
        }
        // sort options by the highest number representation
        std::sort(subdirs.begin(), subdirs.end(), [](const path &a, const path &b) {
            return std::stoi(a.filename().string()) > std::stoi(b.filename().string());
        });
    }
}

// This class is only declared and defined here, in the .cpp file,
// because it is strictly an implementation detail of the top-level
// File_Per_Unit_Saver class
class File_Per_Unit_Snapshot_Saver : public State_Snapshot_Saver
{
    friend class File_Per_Unit_Saver;

    public:
    File_Per_Unit_Snapshot_Saver() = delete;
    File_Per_Unit_Snapshot_Saver(path base_path, State_Saver::State_Durability durability);
    ~File_Per_Unit_Snapshot_Saver();

public:
    void save_unit(std::string const& unit_name, boost::span<char const> data) override;
    void finish_saving() override;

private:
    path dir_path_;
};

File_Per_Unit_Saver::File_Per_Unit_Saver(std::string base_path)
    : base_path_(std::move(base_path))
{
    auto dir_path = path(base_path_);
    create_directories(dir_path);
}

File_Per_Unit_Saver::~File_Per_Unit_Saver() = default;

std::shared_ptr<State_Snapshot_Saver> File_Per_Unit_Saver::initialize_snapshot(State_Durability durability) {
    return std::make_shared<File_Per_Unit_Snapshot_Saver>(path(this->base_path_), durability);
}

std::shared_ptr<State_Snapshot_Saver> File_Per_Unit_Saver::initialize_checkpoint_snapshot(int step, State_Durability durability)
{
    path checkpoint_path = path(this->base_path_) / std::to_string(step);
    create_directory(checkpoint_path);
    return std::make_shared<File_Per_Unit_Snapshot_Saver>(checkpoint_path, durability);
}

void File_Per_Unit_Saver::clear_cache(int mpi_rank) {
    if (mpi_rank == 0) { // reserve file system deletion to just the main MPI rank
        std::vector<path> subdirs;
        ordered_checkpoint_subfolders(this->base_path_, subdirs);
        // delete all checkpoint directories save the most recent one
        for (int i = 1; i < subdirs.size(); ++i) {
            remove_all(subdirs[i]);
        }
    }
}

void File_Per_Unit_Saver::finalize()
{
    // nothing to be done
}

File_Per_Unit_Snapshot_Saver::File_Per_Unit_Snapshot_Saver(path base_path, State_Saver::State_Durability durability)
    : State_Snapshot_Saver(durability)
    , dir_path_(base_path)
{
    create_directory(dir_path_);
}

File_Per_Unit_Snapshot_Saver::~File_Per_Unit_Snapshot_Saver() = default;

void File_Per_Unit_Snapshot_Saver::save_unit(std::string const& unit_name, boost::span<char const> data)
{
    auto file_path = dir_path_ / unit_name;
    try {
        std::ofstream stream(file_path.string(), std::ios_base::out | std::ios_base::binary);
        stream.write(data.data(), data.size());
        stream.close();
    } catch (std::exception &e) {
        LOG("Failed to write state save data for unit '" + unit_name + "' in file '" + file_path.string() + "'", LogLevel::WARNING);
        throw;
    }
}

void File_Per_Unit_Snapshot_Saver::finish_saving()
{
    if (durability_ == State_Saver::State_Durability::strict) {
        // fsync() or whatever
    }
}


// This class is only declared and defined here, in the .cpp file,
// because it is strictly an implementation detail of the top-level
// File_Per_Unit_Saver class
class File_Per_Unit_Snapshot_Loader : public State_Snapshot_Loader
{
    friend class State_Snapshot_Loader;
public:
    File_Per_Unit_Snapshot_Loader() = default;
    File_Per_Unit_Snapshot_Loader(path dir_path);
    ~File_Per_Unit_Snapshot_Loader() override = default;

    bool has_unit(const std::string &unit_name) override;

    /**
     * Load data from whatever source and store it in the `data` vector.
     * 
     * @param data The location where the loaded data will be stored. This will be resized to the amount of data loaded.
     */
    void load_unit(const std::string &unit_name, std::vector<char> &data) override;

    /**
     * Execute logic to complete the saving process
     *
     * Data may be flushed here, and delayed errors may be detected
     * and reported here. With relaxed durability, error reports may
     * not come until the parent State_Saver::finalize() call is made,
     * or ever.
     */
    void finish_saving() override { };

private:
    path dir_path_;
    std::vector<char> data_;
};

File_Per_Unit_Snapshot_Loader::File_Per_Unit_Snapshot_Loader(path dir_path)
    : dir_path_(dir_path)
{

}

bool File_Per_Unit_Snapshot_Loader::has_unit(const std::string &unit_name) {
    auto file_path = dir_path_ / unit_name;
    return exists(file_path.string());
}

void File_Per_Unit_Snapshot_Loader::load_unit(std::string const& unit_name, std::vector<char> &data) {
    auto file_path = dir_path_ / unit_name;
    std::uintmax_t size;
    try {
        size = file_size(file_path.string());
    } catch (std::exception &e) {
        LOG("Failed to read state save data size for unit '" + unit_name + "' in file '" + file_path.string() + "'", LogLevel::WARNING);
        throw;
    }
    std::ifstream stream(file_path.string(), std::ios_base::binary);
    if (!stream) {
        LOG("Failed to open state save data for unit '" + unit_name + "' in file '" + file_path.string() + "'", LogLevel::WARNING);
        throw;
    }
    try {
        data.resize(size);
        stream.read(data.data(), size);
    } catch (std::exception &e) {
        LOG("Failed to read state save data for unit '" + unit_name + "' in file '" + file_path.string() + "'", LogLevel::WARNING);
        throw;
    }
}

File_Per_Unit_Loader::File_Per_Unit_Loader(std::string dir_path)
    : dir_path_(dir_path)
{

}

std::shared_ptr<State_Snapshot_Loader> File_Per_Unit_Loader::initialize_snapshot()
{
    return std::make_shared<File_Per_Unit_Snapshot_Loader>(path(dir_path_));
}

std::shared_ptr<State_Snapshot_Loader> File_Per_Unit_Loader::initialize_checkpoint_snapshot(const std::vector<std::string> &required_units)
{
    std::vector<path> options;
    ordered_checkpoint_subfolders(this->dir_path_, options);
    for (const path &option : options) {
        auto loader = std::make_shared<File_Per_Unit_Snapshot_Loader>(option);
        bool passes = true;
        for (const auto &unit : required_units) {
            if (!loader->has_unit(unit)) {
                passes = false;
                break;
            }
        }
        if (passes) {
            LOG(LogLevel::INFO, "Loading state from checkpoint step " + option.filename().string());
            return loader;
        }
    }
    std::string error = "No checkpoint location found with all required units in root directory " + this->dir_path_;
    LOG(LogLevel::FATAL, error);
    throw std::runtime_error(error);
}


#ifndef NGEN_FILE_PER_UNIT_HPP
#define NGEN_FILE_PER_UNIT_HPP

#include <state_save_restore/State_Save_Restore.hpp>

class File_Per_Unit_Saver : public State_Saver
{
public:
    File_Per_Unit_Saver(std::string base_path);
    ~File_Per_Unit_Saver();

    std::shared_ptr<State_Snapshot_Saver> initialize_snapshot(State_Durability durability) override;

    std::shared_ptr<State_Snapshot_Saver> initialize_checkpoint_snapshot(int step, State_Durability durability) override;

    void clear_cache(int mpi_rank) override;

    void finalize() override;

private:
    std::string base_path_;
};


class File_Per_Unit_Loader : public State_Loader
{
public:
    File_Per_Unit_Loader(std::string dir_path);
    ~File_Per_Unit_Loader() = default;

    void finalize() override { };

    std::shared_ptr<State_Snapshot_Loader> initialize_snapshot() override;

    std::shared_ptr<State_Snapshot_Loader> initialize_checkpoint_snapshot(const std::vector<std::string> &required_units) override;
private:
    std::string dir_path_;
};

#endif // NGEN_FILE_PER_UNIT_HPP

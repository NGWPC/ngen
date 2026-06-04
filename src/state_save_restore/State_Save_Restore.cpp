#include <state_save_restore/State_Save_Restore.hpp>
#include <state_save_restore/File_Per_Unit.hpp>
#include <state_save_restore/vecbuf.hpp>

#include "Logger.hpp"

#include <boost/optional.hpp>
#include <boost/property_tree/ptree.hpp>

#include <string>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

State_Save_Config::State_Save_Config(boost::property_tree::ptree const& tree)
{
    auto maybe = tree.get_child_optional("state_saving");

    // Default initialization will represent the "not enabled" case
    if (!maybe) {
        LOG("State saving not configured", LogLevel::INFO);
        return;
    }

    bool hot_start = false;
    bool checkpoint_load = false;
    bool checkpoint_save = false;
    for (const auto& saving_config : *maybe) {
        try {
            auto& subtree = saving_config.second;
            auto direction = subtree.get<std::string>("direction");
            auto what = subtree.get<std::string>("label");
            auto where = subtree.get<std::string>("path");
            auto how = subtree.get<std::string>("type");
            auto when = subtree.get<std::string>("when");
            auto frequency = subtree.get_optional<int>("frequency");

            instance i{direction, what, where, how, when, frequency};
            if (i.timing_ == State_Save_When::StartOfRun && i.direction_ == State_Save_Direction::Load) {
                if (hot_start)
                    throw std::runtime_error("Only one hot start state saving configuration is allowed.");
                hot_start = true;
            }
            if (i.timing_ == State_Save_When::Checkpoint) {
                if (i.direction_ == State_Save_Direction::Load) {
                    if (checkpoint_load)
                        throw std::runtime_error("Only one checkpointing load state saving configuration is allowed.");
                    checkpoint_load = true;
                } else if (i.direction_ == State_Save_Direction::Save) {
                    if (checkpoint_save)
                        throw std::runtime_error("Only one checkpointing save state saving configuration is allowed.");
                    checkpoint_save = true;
                }
            }
            instances_.push_back(i);
        } catch (std::exception &e) {
            LOG("Bad state saving config: " + std::string(e.what()), LogLevel::WARNING);
            throw;
        }
    }

    LOG("State saving configured", LogLevel::INFO);
}

std::vector<std::pair<std::string, std::shared_ptr<State_Loader>>> State_Save_Config::start_of_run_loaders() const {
    std::vector<std::pair<std::string, std::shared_ptr<State_Loader>>> loaders;
    for (const auto &i : this->instances_) {
        if (i.timing_ == State_Save_When::StartOfRun && i.direction_ == State_Save_Direction::Load) {
            if (i.mechanism_ == State_Save_Mechanism::FilePerUnit) {
                auto loader = std::make_shared<File_Per_Unit_Loader>(i.path_);
                auto pair = std::make_pair(i.label_, loader);
                loaders.push_back(pair);
            } else {
                LOG(LogLevel::WARNING, "State_Save_Config: Loading mechanism " + i.mechanism_string() + " is not supported for start of run loading.");
            }
        }
    }
    return loaders;
}

std::vector<std::pair<std::string, std::shared_ptr<State_Saver>>> State_Save_Config::end_of_run_savers() const {
    std::vector<std::pair<std::string, std::shared_ptr<State_Saver>>> savers;
    for (const auto &i : this->instances_) {
        if (i.timing_ == State_Save_When::EndOfRun && i.direction_ == State_Save_Direction::Save) {
            if (i.mechanism_ == State_Save_Mechanism::FilePerUnit) {
                auto saver = std::make_shared<File_Per_Unit_Saver>(i.path_);
                auto pair = std::make_pair(i.label_, saver);
                savers.push_back(pair);
            } else {
                LOG(LogLevel::WARNING, "State_Save_Config: Saving mechanism " + i.mechanism_string() + " is not supported for start of run saving.");
            }
        }
    }
    return savers;
}

std::unique_ptr<State_Loader> State_Save_Config::hot_start() const {
    for (const auto &i : this->instances_) {
        if (i.direction_ == State_Save_Direction::Load && i.timing_ == State_Save_When::StartOfRun) {
            if (i.mechanism_ == State_Save_Mechanism::FilePerUnit) {
                return std::make_unique<File_Per_Unit_Loader>(i.path_);
            } else {
                LOG(LogLevel::WARNING, "State_Save_Config: Saving mechanism " + i.mechanism_string() + " is not supported for start of run saving.");
            }
        }
    }
    return NULL;
}

std::unique_ptr<State_Loader> State_Save_Config::checkpoint_loader() const {
    for (const auto &i : this->instances_) {
        if (i.direction_ == State_Save_Direction::Load && i.timing_ == State_Save_When::Checkpoint) {
            if (i.mechanism_ == State_Save_Mechanism::FilePerUnit) {
                return std::make_unique<File_Per_Unit_Loader>(i.path_);
            } else {
                std::stringstream ss;
                ss << "State_Save_Config: Loading mechanism " << i.mechanism_string() << " is not supported for checkpoint loading.";
                LOG(LogLevel::FATAL, ss.str());
                throw std::runtime_error(ss.str());
            }
        }
    }
    return NULL;
}

bool State_Save_Config::has_checkpoint_saver() const {
    for (const auto &i : this->instances_) {
        if (i.direction_ == State_Save_Direction::Save && i.timing_ == State_Save_When::Checkpoint) {
            return true;
        }
    }
    return false;
}

std::shared_ptr<State_Saver> State_Save_Config::checkpoint_saver(int *const frequency) const {
    for (const auto &i : this->instances_) {
        if (i.direction_ == State_Save_Direction::Save && i.timing_ == State_Save_When::Checkpoint) {
            if (i.mechanism_ == State_Save_Mechanism::FilePerUnit) {
                *frequency = i.frequency_;
                return std::make_shared<File_Per_Unit_Saver>(i.path_);
            } else {
                std::stringstream ss;
                ss << "State_Save_Config: Saving mechanism " << i.mechanism_string() << " is not supported for checkpoint saving.";
                LOG(LogLevel::FATAL, ss.str());
                throw std::runtime_error(ss.str());
            }
        }
    }
    const auto error = "State_Save_Config: Failed to find a suitable checkpoint saving configuration.";
    LOG(LogLevel::FATAL, error);
    throw std::runtime_error(error);
}

State_Save_Config::instance::instance(std::string const& direction, std::string const& label, std::string const& path, std::string const& mechanism, std::string const& timing, boost::optional<int> frequency)
    : label_(label)
    , path_(path)
    , frequency_{-1}
{
    if (direction == "save") {
        direction_ = State_Save_Direction::Save;
    } else if (direction == "load") {
        direction_ = State_Save_Direction::Load;
    } else {
        std::string message = "Unrecognized state saving direction '" + direction + "'";
        std::string throw_msg; throw_msg.assign(message);
        LOG(throw_msg, LogLevel::WARNING);
        throw std::runtime_error(throw_msg);
    }

    if (mechanism == "FilePerUnit") {
        mechanism_ = State_Save_Mechanism::FilePerUnit;
    } else {
        std::string message = "Unrecognized state saving mechanism '" + mechanism + "'";
        std::string throw_msg; throw_msg.assign(message);
        LOG(throw_msg, LogLevel::WARNING);
        throw std::runtime_error(throw_msg);
    }

    if (timing == "EndOfRun") {
        timing_ = State_Save_When::EndOfRun;
    } else if (timing == "StartOfRun") {
        timing_ = State_Save_When::StartOfRun;
    } else if (timing == "Checkpoint") { // starts with "Checkpoint"
        timing_ = State_Save_When::Checkpoint;
        if (direction_ == State_Save_Direction::Save) {
            if (!frequency.has_value()) {
                const auto message = "The checkpoint save configuration is missing a 'frequency' of checkpointing.";
                LOG(LogLevel::FATAL, message);
                throw std::runtime_error(message);
            }
            frequency_ = frequency.get();
            // make sure the frequency makes sense
            if (frequency_ <= 0) {
                std::stringstream ss;
                ss << "The frequency of a checkpoint save must be greater than 0. The configured value is " << frequency_;
                LOG(LogLevel::FATAL, ss.str());
                throw std::runtime_error(ss.str());
            }
        }
    } else {
        std::string message = "Unrecognized state saving timing '" + timing + "'";
        std::string throw_msg; throw_msg.assign(message);
        LOG(throw_msg, LogLevel::WARNING);
        throw std::runtime_error(throw_msg);
    }
}

std::string State_Save_Config::instance::instance::mechanism_string() const {
    switch (mechanism_) {
        case State_Save_Mechanism::None:
            return "None";
        case State_Save_Mechanism::FilePerUnit:
            return "FilePerUnit";
        default:
            return "Other";
    }
}

State_Snapshot_Saver::State_Snapshot_Saver(State_Saver::State_Durability durability)
    : durability_(durability)
{

}

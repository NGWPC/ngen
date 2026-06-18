#ifndef PAYLOAD_CONFIG_HPP
#define PAYLOAD_CONFIG_HPP

#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>

//Struct to write Payload messages for RTE status communications
struct PayloadConfig {
    std::string model_name;
    std::string prog_status; 
    std::string prog_msg;
    double progress = -1.0; 
    bool has_msg = false;
    bool progress_updated = false;
    std::vector<std::string> payload_model_keys;
        
    // Set payload values - model name and status
    void set_payload_values(const std::string& name, const std::string& status) {
        model_name = name;
        prog_status = status;
        std::string tag = model_name + "|" + prog_status;
        auto it = std::find(payload_model_keys.begin(), payload_model_keys.end(), tag);
        if (it == payload_model_keys.end()) {
            payload_model_keys.push_back(tag);
        }
    }

    // Set custom message in payload message
    void set_custom_msg(const std::string& msg) {
        prog_msg = msg;
        if(!msg.empty()) has_msg = true;
    }

    // Set flag for reported progress
    void set_progress_reported(bool prog_reported) {
        progress_updated = prog_reported;
    }

    void set_progress_percent(double pct) {
        progress = pct;
    }
    
    // Gather models list after simulations are complete
    std::vector<std::string> set_payload_complete()
    {
        std::vector<std::string> completed_models;
        for (const std::string& item : payload_model_keys) {
            if (item.find("IN_PROGRESS") != std::string::npos) {
                std::stringstream ss(item);
                std::string model_name;
                if (std::getline(ss, model_name, '|')) {
                    completed_models.push_back(model_name); // Save the first token
                }
            }
        }
        return completed_models;
    }

    // Return a flag to indicate whether progress is updated.
    bool is_progress_reported() {
        return progress_updated;
    }

    // Check if payload key exists. Required not to repeat the same message for each catchment.
    bool payload_key_exists(std::string tag){
        auto it = std::find(payload_model_keys.begin(), payload_model_keys.end(), tag);
        if (it == payload_model_keys.end()) {
            return true;
        }
        else{
            return false;
        }
    }

    //Reset all variables after simulations are complete
    void reset(){
        model_name = "";
        prog_status = ""; 
        prog_msg = "";
        progress = -1.0; 
        has_msg = false;
        progress_updated = false;
        payload_model_keys.clear();
        payload_model_keys.shrink_to_fit();
    }

    //construct payload string for logging
    std::string construct_payload_string() const {
        std::stringstream ss;
        ss << "\"status\": \"" << prog_status << "\", "; 
        ss << "\"prog\": null, ";
        ss << "\"msg\": ";
        if (has_msg) {
            ss << "\"" << prog_msg << "\"";
        }
        else{
            ss << "null" << ", ";
        }
        ss << "\"modnm\": \"" << model_name << "\"";   
        return "<MSG_DATA>{" + ss.str() + "}</MSG_DATA>";
    }

    //construct payload string for logging
    std::string construct_payload_string(std::string model) const {
        std::stringstream ss;
        ss << "\"status\": \"COMPLETE\", "; 
        ss << "\"prog\": 1.0, ";
        ss << "\"msg\": \"Simulation runs completed\", ";
        ss << "\"modnm\": \"" << model << "\"";
        return "<MSG_DATA>{" + ss.str() + "}</MSG_DATA>";
    }
};

//Global access
inline PayloadConfig& get_pld_config() {
    static PayloadConfig pld; 
    return pld;
}

//Inline functions for writing payload messages
inline void update_payload_config(const std::string& name, const std::string& status, const std::string& msg = "") {
    // Add validation here
    get_pld_config().set_payload_values(name, status);
    get_pld_config().set_custom_msg(msg);
}

inline void run_progress_updated(bool progress_update, double pct = -1.0)
{
    get_pld_config().set_progress_reported(progress_update);

}

inline bool write_payload_msg(const std::string& tag){
    if (tag.find("IN_PROGRESS") != std::string::npos) {
        return get_pld_config().is_progress_reported() && get_pld_config().payload_key_exists(tag);
    } else {
        return get_pld_config().payload_key_exists(tag);
    }

}

inline std::vector<std::string> set_progress_complete(){
    return get_pld_config().set_payload_complete();
}

inline std::string generate_payload_msg(const std::string& model = "") {
    if(model.empty()){
        return get_pld_config().construct_payload_string();
    }
    else return get_pld_config().construct_payload_string(model);
}

inline void reset_payload_attributes(){
    get_pld_config().reset();
}
#endif // PAYLOAD_CONFIG_HPP

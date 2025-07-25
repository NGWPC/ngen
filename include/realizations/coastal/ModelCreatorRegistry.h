#ifndef MODEL_CREATOR_REGISTRY
#define MODEL_CREATOR_REGISTRY

#include <memory>
#include <unordered_map>
#include "realizations/coastal/ModelCreator.h"
#include "realizations/coastal/Coastal_Config_Params.h"

// This is singleton class.
class ModelCreatorRegistry {
public:
    static ModelCreatorRegistry& getInstance();

    void registerCreator(ModelType type,
                    std::unique_ptr<ModelCreator> creator);

    std::unique_ptr<ModelCreator> getCreator(ModelType type) const;

private:
    ModelCreatorRegistry() = default;
    std::unordered_map<ModelType, std::unique_ptr<ModelCreator>> creators;
};

#endif // #ifndef MODEL_CREATOR_REGISTRY


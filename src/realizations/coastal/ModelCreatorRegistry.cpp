#include "ModelCreatorRegistry.hpp"

ModelCreatorRegistry& ModelCreatorRegistry::getInstance() {
    static ModelCreatorRegistry instance;
    return instance;
}

void ModelCreatorRegistry::registerCreator(ModelType type,
                                           std::unique_ptr<ModelCreator> creator) {
    creators[type] = move(creator);
}

std::unique_ptr<ModelCreator>
ModelCreatorRegistry::getCreator(ModelType type) const {
    auto it = creators.find(type);
    if (it != creators.end()) {
        return std::unique_ptr<ModelCreator>(it->second->clone());
    }
    return nullptr;
}

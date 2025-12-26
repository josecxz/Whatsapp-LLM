#include "message_validator.h"
#include <stdexcept>

void validate_message(const nlohmann::json& msg) {
    const char* required[] = {
        "id", "chat_jid", "sender",
        "content", "timestamp", "is_from_me"
    };

    for (auto& field : required) {
        if (!msg.contains(field)) {
            throw std::runtime_error(
                std::string("Missing field: ") + field
            );
        }
    }

    if (!msg["is_from_me"].is_boolean()) {
        throw std::runtime_error("is_from_me must be boolean");
    }
}

#include "generators/identity/registerIdentityGenerators.hpp"

#include "generators/identity/generators/AccountIdGenerator.hpp"
#include "generators/identity/generators/DisplayNameGenerator.hpp"
#include "generators/identity/generators/EmailLikeIdentifierGenerator.hpp"
#include "generators/identity/generators/NicknameGenerator.hpp"
#include "generators/identity/generators/ProfileMetadataGenerator.hpp"
#include "generators/identity/generators/UsernameGenerator.hpp"

#include <memory>

namespace account_recommendations::generators::identity
{

common::Result<void, domain::RegistryError>
registerIdentityGenerators(domain::GeneratorRegistry& registry)
{
    using R = common::Result<void, domain::RegistryError>;
    const auto add = [&registry](std::unique_ptr<domain::IGenerator> generator)
        -> common::Result<void, domain::RegistryError> {
        return registry.registerGenerator(std::move(generator));
    };

    if (auto result = add(std::make_unique<UsernameGenerator>()); !result)
        return R::failure(std::move(result).error());
    if (auto result = add(std::make_unique<DisplayNameGenerator>()); !result)
        return R::failure(std::move(result).error());
    if (auto result = add(std::make_unique<NicknameGenerator>()); !result)
        return R::failure(std::move(result).error());
    if (auto result = add(std::make_unique<EmailLikeIdentifierGenerator>()); !result)
        return R::failure(std::move(result).error());
    if (auto result = add(std::make_unique<AccountIdGenerator>()); !result)
        return R::failure(std::move(result).error());
    if (auto result = add(std::make_unique<ProfileMetadataGenerator>()); !result)
        return R::failure(std::move(result).error());
    return R::success();
}

} // namespace account_recommendations::generators::identity

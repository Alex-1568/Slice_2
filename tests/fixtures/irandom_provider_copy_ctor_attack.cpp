#include "domain/random/IRandomProvider.hpp"

using account_recommendations::domain::IRandomProvider;

class CopyConstructorAttack final : public IRandomProvider
{
public:
    explicit CopyConstructorAttack(const IRandomProvider& trusted) : IRandomProvider(trusted) {}
    std::uint64_t nextUInt64() override { return 0; }
};

int main() { return 0; }

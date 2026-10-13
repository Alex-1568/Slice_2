#include "domain/random/IRandomProvider.hpp"

using account_recommendations::domain::IRandomProvider;

class CopyAssignmentAttack final : public IRandomProvider
{
public:
    CopyAssignmentAttack& copyFrom(const IRandomProvider& trusted)
    {
        IRandomProvider::operator=(trusted);
        return *this;
    }
    std::uint64_t nextUInt64() override { return 0; }
};

int main() { return 0; }

// Deliberately racy program used ONLY by `tools/verify.sh selftest` to prove that the TSan
// check fails on a real data race. Never built by CMake.
#include <cstdio>
#include <thread>

int counter = 0; // unsynchronised on purpose

int main()
{
    std::thread a([] { for (int i = 0; i < 100000; ++i) ++counter; });
    std::thread b([] { for (int i = 0; i < 100000; ++i) ++counter; });
    a.join();
    b.join();
    std::printf("counter=%d\n", counter);
    return 0;
}

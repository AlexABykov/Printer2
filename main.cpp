#include "Tests/Tests.hpp"

int main() {
    const int failed = Tests::runAll();   // all tests run from main (spec §12, §13.41)
    return failed == 0 ? 0 : 1;
}

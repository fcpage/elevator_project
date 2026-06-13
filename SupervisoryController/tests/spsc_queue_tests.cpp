#include <cstdlib>
#include <iostream>

#include "supervisory/common/spsc_queue.hpp"

namespace
{

void require(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "TEST_FAILURE " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

} // namespace

int main()
{
    project6::supervisory::cSpscQueue<int, 2> queue;
    int value = 0;

    require(queue.tryPush(10), "first value was rejected");
    require(queue.tryPush(20), "second value was rejected");
    require(!queue.tryPush(30), "full queue accepted another value");

    require(queue.tryPop(value) && value == 10, "first value was not FIFO");
    require(queue.tryPop(value) && value == 20, "second value was not FIFO");
    require(!queue.tryPop(value), "empty queue returned a value");

    return 0;
}

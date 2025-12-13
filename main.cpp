#include "ThreadPool.hpp"
#include <iostream>

int main()
{
    qinmo::ThreadPool threadPool(4);

    std::vector<std::future<int>> result;
    for (int i = 0; i < 4; ++i)
    {
        result.push_back(std::move(threadPool.submit([](int i) -> int { return i; } , i)));
    }
    for (auto&& fut : result)
    {
        std::cout << fut.get() << "\n";
    }
    std::cout << std::endl;

    for (int i = 0; i < 4; ++i)
    {
        threadPool.submit([]() -> void { std::cout << "hello world.\n"; });
    }

    threadPool.shutdown();
    return 0;

    /*
    output:

    0
    1
    2
    3
    
    hello world.
    hello world.
    hello world.
    hello world.
    */
}
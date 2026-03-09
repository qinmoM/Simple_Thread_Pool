#include "ThreadPool.hpp"
#include <iostream>

int main()
{
    // create a thread pool with a maximum of 4 concurrent threads
    qinmo::ThreadPool threadPool(4);


    // submit task (non-blocking)
    for (int i = 0; i < 4; ++i)
        threadPool.submit([]() -> void { std::cout << "hello world.\n"; });


    // create an array for asynchronous retrieval of return values
    std::vector<std::future<int>> result;

    // submit four rasks to the thread pool (non-blocking)
    for (int i = 0; i < 4; ++i)
        result.push_back(std::move(threadPool.submit([](int i) -> int { return i; } , i)));

    // print their return values (blocking)
    for (auto&& fut : result)
        std::cout << fut.get() << "\n";

    std::cout << std::endl;


    // manual shutdown
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
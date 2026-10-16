#include "ThreadPool.hpp"
#include "lockfree/MPMCUnbounded.h"
#include <iostream>
#include <windows.h>

int main()
{
    // // create a thread pool with a maximum of 4 concurrent threads
    // qinmo::ThreadPool threadPool(4);


    // // submit task (non-blocking)
    // for (int i = 0; i < 4; ++i)
    //     threadPool.submit([]() -> void { std::cout << "hello world.\n"; });


    // // create an array for asynchronous retrieval of return values
    // std::vector<std::future<int>> result;

    // // submit four rasks to the thread pool (non-blocking)
    // for (int i = 0; i < 4; ++i)
    //     result.push_back(std::move(threadPool.submit([](int i) -> int { return i; } , i)));

    // // print their return values (blocking)
    // for (auto&& fut : result)
    //     std::cout << fut.get() << "\n";

    // std::cout << std::endl;


    // // manual shutdown
    // threadPool.shutdown();




    // lock-free
    // qinmo::println("hello");

    // MPMCBounded<int> q(10);
    // while (true)
    // {
    //     for (int i = 0; i < 12; ++i)
    //         qinmo::println(q.push(i));

    //     int temp = 0;
    //     for (int i = 0; i < 12; ++i)
    //         qinmo::println(q.pop(temp), ' ', temp, '\t');

    //     Sleep(1000);
    // }
    MPMCBounded<int> q(3);
    std::atomic<bool> stop(false);
    int i = 0;

    std::thread th1(
        [&q, &i, &stop]() -> void
        {
            while (!stop)
            {
                if (q.push(i++))
                    qinmo::println("success");
                else
                    qinmo::println("failed");

                Sleep(1000);
            }
        }
    );
    std::thread th2(
        [&q, &stop]() -> void
        {
            while (!stop)
            {
                int temp = 0;
                while (!q.pop(temp));
                qinmo::println(temp);

                Sleep(1100);
            }
        }
    );

    while (!stop);
    th1.join();
    th2.join();

    return 0;

    /*
    output:
    
    hello world.
    hello world.
    hello world.
    hello world.
    0
    1
    2
    3
    */
}
/**
 *          speed test
*/



/**         test result
 * 
 * debug output:
 * ordinary time: 1501880                   (1501ms)
 * thread time: 206011                      (206ms)
 * thread pool time: 211305                 (211ms)
 * thread pool time(Uninitialized): 207068  (207ms)
 * 
 * release output:
 * ordinary time: 200945                    (201ms)
 * thread time: 37036                       (37ms)
 * thread pool time: 44626                  (45ms)
 * thread pool time(Uninitialized): 39151   (39ms)
*/



//          test code

#include "../include/ThreadPool.hpp"
#include <chrono>
#include <algorithm>
#include <iostream>

struct Task
{
    void operator()(std::vector<int>& data)
    {
        std::sort(data.begin(), data.end());
    }
};

int main()
{
    using clock = std::chrono::high_resolution_clock;
    std::vector<std::vector<int>> data1(10, std::vector<int>(1000000, 0));
    std::vector<std::vector<int>> data2(10, std::vector<int>(1000000, 0));
    std::vector<std::vector<int>> data3(10, std::vector<int>(1000000, 0));
    std::vector<std::vector<int>> data4(10, std::vector<int>(1000000, 0));
    for (int i = 0; i < 1000000; ++i)
    {
        for (int j = 0; j < 10; ++j)
        {
            data1[j][i] = i >> 4 + i << 4 + j << 2 + j >> 2;
            data2[j][i] = data1[j][i];
            data3[j][i] = data1[j][i];
            data4[j][i] = data1[j][i];
        }
    }
    // ordinary
    auto start = clock::now();
    for (int i = 0; i < 10; ++i)
    {
        Task task;
        task(data1[i]);
    }
    std::cout << "ordinary time: " << std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start).count() << std::endl;
    // thread
    start = clock::now();
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back(Task(), std::ref(data2[i]));
    }
    for (auto& thread : threads)
    {
        if (thread.joinable()) thread.join();
    }
    std::cout << "thread time: " << std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start).count() << std::endl;
    // thread pool
    start = clock::now();
    qinmo::ThreadPool threadpool(10);
    for (int i = 0; i < 10; ++i)
    {
        threadpool.submit(Task(), data3[i]);
    }
    threadpool.shutdown();
    std::cout << "thread pool time: " << std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start).count() << std::endl;
    // thread pool(Uninitialized)
    qinmo::ThreadPool threadpool_u(10);
    start = clock::now();
    for (int i = 0; i < 10; ++i)
    {
        threadpool_u.submit(Task(), data4[i]);
    }
    threadpool_u.shutdown();
    std::cout << "thread pool time(Uninitialized): " << std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start).count() << std::endl;
    return 0;
}

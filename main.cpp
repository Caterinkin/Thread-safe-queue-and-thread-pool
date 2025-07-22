#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <algorithm>

/**
 * ��������� ����� ���������������� �������
 */
template<typename T>
class safe_queue
{
private:
    // ������� ��� �������� ���������
    std::queue<T> queue;

    // ������� ��� ������������� �������
    std::mutex mutex;

    // �������� ���������� ��� �����������
    std::condition_variable cond_var;

public:
    /**
     * ����������� �� ���������
     */
    safe_queue() = default;

    /**
     * ����������
     */
    ~safe_queue() = default;

    /**
     * ���������� �������� � �������
     * @param value - ������� ��� ����������
     */
    void push(T value)
    {
        // ��������� �������
        std::unique_lock<std::mutex> lock(mutex);

        // ��������� ������� � �������
        queue.push(std::move(value));

        // ������������ ������� ����� ������������
        lock.unlock();

        // ���������� ���� ��������� �����
        cond_var.notify_one();
    }

    /**
     * ���������� �������� �� �������
     * @return ����������� �������
     */
    T pop()
    {
        // ��������� �������
        std::unique_lock<std::mutex> lock(mutex);

        // ����, ���� � ������� �������� �������
        cond_var.wait(lock, [this]() { return !queue.empty(); });

        // ��������� ������� �� �������
        T value = std::move(queue.front());
        queue.pop();

        return value;
    }

    /**
     * �������� �� ������� �������
     * @return true, ���� ������� �����
     */
    bool empty()
    {
        // ��������� �������
        std::unique_lock<std::mutex> lock(mutex);

        return queue.empty();
    }
};

/**
 * ����� ���� �������
 */
class thread_pool
{
private:
    // ���� ��������� ����
    bool stop_pool = false;

    // ������ ������� �������
    std::vector<std::thread> workers;

    // ���������������� ������� �����
    safe_queue<std::function<void()>> tasks;

    // ������� ��� ������������� ������
    std::mutex cout_mutex;

public:
    /**
     * �����������
     * @param num_threads - ���������� ������� � ����
     */
    explicit thread_pool(size_t num_threads = std::thread::hardware_concurrency())
    {
        // ������� ������� ������
        for (size_t i = 0; i < num_threads; ++i)
        {
            workers.emplace_back([this] { work(); });
        }
    }

    /**
     * ����������
     */
    ~thread_pool()
    {
        // ������������� ���� ���������
        stop_pool = true;

        // ���������� ���������� ���� �������
        for (auto& worker : workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

    /**
     * ����� ��� ���������� ������ � ���
     * @param func - ������ ��� ����������
     * @return future ��� ��������� ����������
     */
    template<typename Func>
    auto submit(Func func) -> std::future<decltype(func())>
    {
        // ������� packaged_task ��� �������
        auto task = std::make_shared<std::packaged_task<decltype(func())()>>(std::move(func));

        // �������� future ��� ����������
        auto result = task->get_future();

        // ��������� ������ � �������
        tasks.push([task]() { (*task)(); });

        return result;
    }

private:
    /**
     * �����, ����������� ������ ������� �������
     */
    void work()
    {
        while (!stop_pool)
        {
            try
            {
                // ��������� ������ �� �������
                auto task = tasks.pop();

                // ��������� ������
                task();
            }
            catch (const std::exception& e)
            {
                // ��������� ����� ��� ����������� ����������� ������
                std::unique_lock<std::mutex> lock(cout_mutex);
                std::cerr << "������ � ������� ������: " << e.what() << std::endl;
            }
        }
    }
};

// �������� ������� ��� ������������ ������ ����
void test_function1()
{
    std::cout << "������� test_function1 ����������� � ������ "
        << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void test_function2()
{
    std::cout << "������� test_function2 ����������� � ������ "
        << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
}

void test_function3(int x)
{
    std::cout << "������� test_function3 � ���������� " << x
        << " ����������� � ������ " << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

int main()
{
    setlocale(LC_ALL, "Rus");
    // ������� ��� �������
    thread_pool pool;

    // ��������� ������ � ��� � ���������� � 1 �������
    for (int i = 0; i < 5; ++i)
    {
        // ��������� ������ ������
        pool.submit(test_function1);

        // ��������� ������ ������
        pool.submit(test_function2);

        // ��������� ������ � ����������
        pool.submit([i]() { test_function3(i); });

        // ���� 1 ������� ����� ����������� ��������� �����
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::cout << "----------------------" << std::endl;
    }

    return 0;
}
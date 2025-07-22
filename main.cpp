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
 * Шаблонный класс потокобезопасной очереди
 */
template<typename T>
class safe_queue
{
private:
    // Очередь для хранения элементов
    std::queue<T> queue;

    // Мьютекс для синхронизации доступа
    std::mutex mutex;

    // Условная переменная для уведомлений
    std::condition_variable cond_var;

public:
    /**
     * Конструктор по умолчанию
     */
    safe_queue() = default;

    /**
     * Деструктор
     */
    ~safe_queue() = default;

    /**
     * Добавление элемента в очередь
     * @param value - элемент для добавления
     */
    void push(T value)
    {
        // Блокируем мьютекс
        std::unique_lock<std::mutex> lock(mutex);

        // Добавляем элемент в очередь
        queue.push(std::move(value));

        // Разблокируем мьютекс перед уведомлением
        lock.unlock();

        // Уведомляем один ожидающий поток
        cond_var.notify_one();
    }

    /**
     * Извлечение элемента из очереди
     * @return извлеченный элемент
     */
    T pop()
    {
        // Блокируем мьютекс
        std::unique_lock<std::mutex> lock(mutex);

        // Ждем, пока в очереди появится элемент
        cond_var.wait(lock, [this]() { return !queue.empty(); });

        // Извлекаем элемент из очереди
        T value = std::move(queue.front());
        queue.pop();

        return value;
    }

    /**
     * Проверка на пустоту очереди
     * @return true, если очередь пуста
     */
    bool empty()
    {
        // Блокируем мьютекс
        std::unique_lock<std::mutex> lock(mutex);

        return queue.empty();
    }
};

/**
 * Класс пула потоков
 */
class thread_pool
{
private:
    // Флаг остановки пула
    bool stop_pool = false;

    // Вектор рабочих потоков
    std::vector<std::thread> workers;

    // Потокобезопасная очередь задач
    safe_queue<std::function<void()>> tasks;

    // Мьютекс для синхронизации вывода
    std::mutex cout_mutex;

public:
    /**
     * Конструктор
     * @param num_threads - количество потоков в пуле
     */
    explicit thread_pool(size_t num_threads = std::thread::hardware_concurrency())
    {
        // Создаем рабочие потоки
        for (size_t i = 0; i < num_threads; ++i)
        {
            workers.emplace_back([this] { work(); });
        }
    }

    /**
     * Деструктор
     */
    ~thread_pool()
    {
        // Устанавливаем флаг остановки
        stop_pool = true;

        // Дожидаемся завершения всех потоков
        for (auto& worker : workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

    /**
     * Метод для добавления задачи в пул
     * @param func - задача для выполнения
     * @return future для получения результата
     */
    template<typename Func>
    auto submit(Func func) -> std::future<decltype(func())>
    {
        // Создаем packaged_task для функции
        auto task = std::make_shared<std::packaged_task<decltype(func())()>>(std::move(func));

        // Получаем future для результата
        auto result = task->get_future();

        // Добавляем задачу в очередь
        tasks.push([task]() { (*task)(); });

        return result;
    }

private:
    /**
     * Метод, выполняемый каждым рабочим потоком
     */
    void work()
    {
        while (!stop_pool)
        {
            try
            {
                // Извлекаем задачу из очереди
                auto task = tasks.pop();

                // Выполняем задачу
                task();
            }
            catch (const std::exception& e)
            {
                // Блокируем вывод для корректного отображения ошибки
                std::unique_lock<std::mutex> lock(cout_mutex);
                std::cerr << "Ошибка в рабочем потоке: " << e.what() << std::endl;
            }
        }
    }
};

// Тестовые функции для демонстрации работы пула
void test_function1()
{
    std::cout << "Функция test_function1 выполняется в потоке "
        << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void test_function2()
{
    std::cout << "Функция test_function2 выполняется в потоке "
        << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
}

void test_function3(int x)
{
    std::cout << "Функция test_function3 с аргументом " << x
        << " выполняется в потоке " << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

int main()
{
    setlocale(LC_ALL, "Rus");
    // Создаем пул потоков
    thread_pool pool;

    // Добавляем задачи в пул с интервалом в 1 секунду
    for (int i = 0; i < 5; ++i)
    {
        // Добавляем первую задачу
        pool.submit(test_function1);

        // Добавляем вторую задачу
        pool.submit(test_function2);

        // Добавляем задачу с аргументом
        pool.submit([i]() { test_function3(i); });

        // Ждем 1 секунду перед добавлением следующих задач
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::cout << "----------------------" << std::endl;
    }

    return 0;
}
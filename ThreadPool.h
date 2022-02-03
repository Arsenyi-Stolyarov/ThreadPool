#ifndef  THREADPOOL_H
#define  THREADPOOL_H


#include <condition_variable>
#include <thread>
#include <mutex>

#include <functional>
#include <vector>
#include <map>


class ThreadPool
{

public:

    using Lock = std::unique_lock <std::mutex>;

    using Task = std::function <void ()>;

    using Size = std::size_t;

private:

    mutable struct : std::mutex, std::condition_variable
    {

        std::vector <std::thread> threads; ///< Очередь задач.

        std::multimap <Size, Task> tasks; ///< Потоки.

        bool stop = true; ///< Флаг остановки пула.

    } pool;

public:

    explicit ThreadPool (Size size = 0) ///< Конструктор.
    {
        init(size);
    }

  ~ ThreadPool () ///< Деструктор.
    {
        stop();
    }


    void add_task (const Task & task, Size priority = (Size) - 1) ///< Добавление задачи. По умолчанию - в конец очереди.
    {
        Lock lock(pool);

        pool.tasks.emplace(priority, task); ///< std::multimap сортирует задачи по приоритету. Для задач с одинаковым приоритетом сохраняется порядок добавления.

        pool.notify_one(); ///< Уведомляем потоки о поступлении задачи.
    }

    Task get_task () ///< Извлечение задачи из начала очереди.
    {
        Lock lock(pool);

        if (pool.tasks.empty()) ///< Если очередь задач пуста - уведомляем об этом потоки нулевым указателем.

            return nullptr;

        return pool.tasks.extract(pool.tasks.begin()).mapped(); ///< Извлекаем и возвращаем задачу из начала очереди.
    }


    void init (Size size) ///< Запуск пула.
    {
        Lock lock(pool);

        if (! pool.stop || ! size) ///< Если уже запущен или если ноль потоков - выходим.

            return;

        pool.stop = false; ///< Снимаем флаг остановки.

        pool.threads.resize(size); ///< Подготавливаем вектор.

        for (auto & thread : pool.threads) ///< Инициализируем вектор.

            thread = std::thread([&] () ///< Жизненный цикл потока.
            {
                Task task;

                while (! exit()) ///< Пока пул работает ...
                {
                    while ((task = get_task())) ///< ... если есть задачи в очереди ...

                        task(); ///< ... выполняем ...

                    rest(); ///< ... отдыхаем.
                }
            });
    }

    void stop () ///< Остановка пула.
    {
        Lock lock(pool);

        if (pool.stop) ///< Если уже остановлен - выходим.

            return;

        pool.wait(lock, [&] () { return pool.tasks.empty(); }); ///< Ждём завершения всех задач в очереди.

        pool.stop = true; ///< Устанавливаем флаг остановки.

        pool.notify_all(); ///< Уведомляем потоки о прекращении работы пула.

        lock.unlock();

        for (auto & thread : pool.threads)

            thread.join(); ///< Дожидаемся завершения работы потоков.

        pool.threads.clear(); ///< Уничтожаем потоки.
    }

    Size size () const ///< Кол-во потоков в пуле.
    {
        Lock lock(pool);

        return pool.threads.size();
    }

private:

    void rest () ///< Переход потока в режим ожидания.
    {
        Lock lock(pool);

        pool.notify_all(); ///< Уведомляем пул о переходе потока в режим ожидания.

        pool.wait(lock, [&] () { return ! pool.tasks.empty() || pool.stop; }); ///< Ждём поступления новых задач или установки флага остановки.
    }

    bool exit () ///< Состояние флага остановки.
    {
        Lock lock(pool);

        return pool.stop;
    }

};


#endif //THREADPOOL_H

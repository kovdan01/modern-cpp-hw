#include <utils.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>

namespace hw2
{

std::shared_ptr<spdlog::logger> logger()
{
    class Singleton
    {
    public:
        static Singleton& get_instance()
        {
            static Singleton instance;
            return instance;
        }

        Singleton(const Singleton& root) = delete;
        Singleton& operator=(const Singleton&) = delete;
        Singleton(Singleton&& root) = delete;
        Singleton& operator=(Singleton&&) = delete;

        std::shared_ptr<spdlog::logger> logger;

    private:
        Singleton()
        {
#ifndef NDEBUG
            spdlog::set_level(spdlog::level::debug);
#else
            spdlog::set_level(spdlog::level::info);
#endif
            logger = spdlog::stdout_logger_mt("console");
            spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [tid %t] [%l] %v");
        }
    };

    return Singleton::get_instance().logger;
}

}  // namespace hw2

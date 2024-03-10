#include <irods/irods_at_scope_exit.hpp>

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/program_options.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <thread>

int main(int _argc, char* _argv[])
{
    namespace po = boost::program_options;

    po::options_description opts_desc{""};

    // clang-format off
    opts_desc.add_options()
        ("config-file,f", po::value<std::string>(), "")
        ("jsonschema-file", po::value<std::string>(), "")
        ("dump-config-template", "")
        ("dump-default-jsonschema", "")
        ("deamonize,d", "")
        ("pid-file", "")
        ("help,h", "")
        ("version,v", "");
    // clang-format on

    po::positional_options_description pod;
    pod.add("config-file", 1);

    using json = nlohmann::json;
    json config;

    try {
        po::variables_map vm;
        po::store(po::command_line_parser(_argc, _argv).options(opts_desc).positional(pod).run(), vm);
        po::notify(vm);

        if (vm.count("help") > 0) {
            //print_usage();
            return 0;
        }

        if (vm.count("version") > 0) {
            //print_version_info();
            return 0;
        }

        if (vm.count("dump-config-template") > 0) {
            //print_configuration_template();
            return 0;
        }

        if (vm.count("dump-default-jsonschema") > 0) {
            //fmt::print(default_jsonschema());
            return 0;
        }

        if (vm.count("config-file") == 0) {
            fmt::print(stderr, "Error: Missing [CONFIG_FILE_PATH] parameter.");
            return 1;
        }

        // TODO Validate configuration.
#if 0
        const auto config = json::parse(std::ifstream{vm["config-file"].as<std::string>()});
        irods::http::globals::set_configuration(config);

        {
            const auto schema_file = (vm.count("jsonschema-file") > 0) ? vm["jsonschema-file"].as<std::string>() : "";
            if (!is_valid_configuration(schema_file, vm["config-file"].as<std::string>())) {
                return 1;
            }
        }
#endif

        // TODO If deamonizing the process fails, we should log it somewhere.
        // Should this happen after loading the configuration?
        if (vm.count("deamonize") > 0) {
            // TODO
        }

        if (vm.count("pid-file") > 0) {
            // TODO
        }
        else {
            // TODO
        }

        // TODO Load configuration for parent process.
    }
    catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return 1;
    }

    try {
        // TODO Init base systems for parent process.
        // - logger
        // - shared memory for replica access table, dns cache, hostname cache?
        // - delay server salt
        
        // This message queue gives child processes a way to notify the parent process.
        constexpr const auto* mq_name = "irods_config_derived_mq_name";
        constexpr auto max_number_of_msg = 10; 
        constexpr auto max_msg_size = 512; 
        boost::interprocess::message_queue::remove(mq_name);
        boost::interprocess::message_queue mq{
            boost::interprocess::create_only, mq_name, max_number_of_msg, max_msg_size};

        // TODO Fork child processes.
        // The child processes should probably be exec'd too.
        // Each child process needs to communicate its status (i.e. init, ready, run, etc).

        // Fork agent factory.
        auto pid_af = fork();
        if (0 == pid_af) {
            // TODO Set up signal handlers.
            // Only the parent should be able to trigger a config reload via SIGHUP.
            // That means each child process must configure a message queue.
            // Or, we learn how to use zeromq for broadcasting instructions. That is all fine and dandy,
            // but it feels good to be able to select which children should be updated, etc.

            std::this_thread::sleep_for(std::chrono::seconds{2});
            const char* msg = "agent factory: hello irodsd";
            mq.send(msg, std::strlen(msg) + 1, 0);

            std::this_thread::sleep_for(std::chrono::seconds{2});
            msg = "agent factory: good bye!";
            mq.send(msg, std::strlen(msg) + 1, 0);

            return 0;
        }
        else if (-1 == pid_af) {
            fmt::print(stderr, "Error: could not fork agent factory.\n");
            return 1;
        }
        fmt::print("agent factory pid = [{}]\n", pid_af);

        // Fork delay server.
        auto pid_ds = fork();
        if (0 == pid_ds) {
            std::this_thread::sleep_for(std::chrono::seconds{2});
            const char* msg = "delay server: hello irodsd";
            mq.send(msg, std::strlen(msg) + 1, 0);

            std::this_thread::sleep_for(std::chrono::seconds{2});
            msg = "delay server: good bye!";
            mq.send(msg, std::strlen(msg) + 1, 0);

            return 0;
        }
        else if (-1 == pid_ds) {
            kill(pid_af, SIGTERM);
            fmt::print(stderr, "Error: could not fork delay server.\n");
            return 1;
        }
        fmt::print("delay server pid = [{}]\n", pid_ds);

        // Fork control plane.
        // Should this be forked first?
        auto pid_cp = fork();
        if (0 == pid_cp) {
            std::this_thread::sleep_for(std::chrono::seconds{2});
            const char* msg = "control plane: hello irodsd";
            mq.send(msg, std::strlen(msg) + 1, 0);

            std::this_thread::sleep_for(std::chrono::seconds{2});
            msg = "control plane: good bye!";
            mq.send(msg, std::strlen(msg) + 1, 0);

            return 0;
        }
        else if (-1 == pid_cp) {
            kill(pid_af, SIGTERM);
            kill(pid_ds, SIGTERM);
            fmt::print(stderr, "Error: could not fork control plane.\n");
            return 1;
        }
        fmt::print("control plane pid = [{}]\n", pid_cp);

        // TODO Init signal handlers.

        // Enter main loop.
        std::array<char, max_msg_size> msg_buf{};
        boost::interprocess::message_queue::size_type recvd_size;
        unsigned int priority;
        for (int msgs_remaining = 6; msgs_remaining > 0; --msgs_remaining) {
            msg_buf.fill(0);
            mq.receive(msg_buf.data(), msg_buf.size(), recvd_size, priority);
            fmt::print("irodsd: received message: [{}]\n", msg_buf.data());
        }

        waitpid(pid_cp, nullptr, 0);
        waitpid(pid_ds, nullptr, 0);
        waitpid(pid_af, nullptr, 0);

        return 0;
    }
    catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return 1;
    }
} // main

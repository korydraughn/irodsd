#include <irods/irods_at_scope_exit.hpp>

#include <boost/asio.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/program_options.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

//#include <sys/types.h>
//#include <sys/wait.h>
//#include <unistd.h>

#include <chrono>
#include <csignal>
#include <string>
#include <string_view>
#include <thread>

int main(int _argc, char* _argv[])
{
    namespace po = boost::program_options;

    po::options_description opts_desc{""};

    // clang-format off
    opts_desc.add_options()
        ("parent-message-queue", po::value<std::string>(), "")
        //("jsonschema-file", po::value<std::string>(), "")
        //("dump-config-template", "")
        //("dump-default-jsonschema", "")
        //("daemonize,d", "")
        //("pid-file", "")
        ("help,h", "")
        ("version,v", "");
    // clang-format on

    po::positional_options_description pod;
    pod.add("parent-message-queue", 1);

    using json = nlohmann::json;
    json config;

    std::string pmq_name;

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

        if (vm.count("parent-message-queue") == 0) {
            fmt::print(stderr, "Error: Missing [PARENT_MESSAGE_QUEUE_NAME");
            return 1;
        }

        pmq_name = vm["parent-message-queue"].as<std::string>();

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

        boost::interprocess::message_queue pproc_mq{boost::interprocess::open_only, pmq_name.c_str()};

        // TODO Set up signal handlers.
        // Only the parent should be able to trigger a config reload via SIGHUP.
        // That means each child process must configure a message queue.
        // Or, we learn how to use zeromq for broadcasting instructions. That is all fine and dandy,
        // but it feels good to be able to select which children should be updated, etc.

        // Send messages until the parent process sends the terminate signal.
        while (true) {
            constexpr std::string_view msg = "agent factory: still accepting new requests!";
            pproc_mq.send(msg.data(), msg.size(), 0);
            std::this_thread::sleep_for(std::chrono::seconds{3});
        }

        return 0;
    }
    catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return 1;
    }
} // main

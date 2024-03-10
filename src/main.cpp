// iRODS 5.0 (Early) Startup Re-design Thoughts
// 
// =======================
// Design Notes & Thoughts
// =======================
//
// - Admin must pass location of config file on startup (just like the HTTP API).
//     - Or, if not passed, the server could assume its running as a non-package-install.
//     - See next bullet item for the option-based approach.
// - Admin can pass an option to indicate the equivalent of running as a non-package install.
//     - This would cause the server to search in the local directory for its configs.
//     - The local directory must have the correct/expected structure/layout.
//
// - Runs as a normal application by default (just like the HTTP API).
// - Admin must pass -d to daemonize the server.
//
// - Only the parent process can react to SIGHUP or signals that cause child processes to "do" something.
//     - Admin can only communicate with parent process.
//
// - ZeroMQ is pretty fancy and could be really promising if we take the time to learn it.
//     - Pro: Supports several styles of message passing.
//     - Pro: Is more powerful than traditional sockets. 
//     - Pro: Is proven and stable.
//     - Con: Will take time to learn.
//     - Con: May be overkill for our needs.
//
// - Parent process
//     - Handles ALL initialization and shutdown.
//         - No more iRODS server logic in Python!
//     - Startup should not prepare shared memory for any plugins.
//         - Plugins are responsible for their own shared memory.
//         - This applies directly to the NREP.
//     - Never allowed to launch threads because it must be ready to fork dead child processes.
//     - Everything must be handled within the main loop.
//     - Could fork AND exec all child processes.
//         - All child processes become normal binaries which the admin can launch.
//             - May not work due to information needed by the parent process (e.g. message queue names, configs, etc).
//     - Only leaf processes are allowed to use multiple threads.
//
// - Agent Factory is responsible for handling client requests.
//     - Parent process is not part of the mix.
//     - Tracks the list of active agents (for ips and irods-grid).
// 
// - Control Plane becomes a forked process.
//     - Listens for incoming requests just like the agent factory.
//     - Supports a limited set of commands: shutdown, status
//     - Can communicate with the Agent Factory to retrieve active PIDs, etc.
//     - Does not use the iRODS RPC APIs to fetch information.
//
// - Delay server migration
//     - Managed in the main loop of the parent process.
//     - Or, the logic is split across the parent process and delay server.
//         - Delay server watches for changes in the leader/successor and shuts down on its own.
//         - Parent process sees delay server shutdown (waitpid) and reacts accordingly.
//
// - Q. Where should PID files go?
//     - Consider packaged vs non-package installs.
// - Q. Where should shared memory live?
//     - Consider packaged vs non-package installs.
//     - Boost.Interprocess doesn't always allow us to choose.
//
// ========================
// Running This Application
// ========================
//
// This implementation uses Boost.Interprocess message_queues for communication between
// processes.
//
// The application launches three child processes, all called "irodsd". To launch, run the following:
//
//     ./irodsd config_file
//
// This will launch four processes.
// - irodsd (parent)
// - irodsd (agent factory)
// - irodsd (delay server)
// - irodsd (control plane)
//
// The parent process reads messages from the message queue and prints them to the terminal.
// The agent factory and delay server post a message to the message queue every few seconds.
// The control plane launches a tiny TCP server which listens on port 9000 for the keyword, "shutdown".
//
// When the control plane receives the "shutdown" instruction, it posts a shutdown message
// to the message queue which causes the parent process to exit the infinite loop and kill the
// child processes.
//
// To see this in action, run the following in a separate terminal (assumes netcat is installed):
//
//     echo shutdown | nc localhost 9000

#include <irods/irods_at_scope_exit.hpp>

#include <boost/asio.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/program_options.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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
        ("config-file,f", po::value<std::string>(), "")
        ("jsonschema-file", po::value<std::string>(), "")
        ("dump-config-template", "")
        ("dump-default-jsonschema", "")
        ("daemonize,d", "")
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

        // TODO If daemonizing the process fails, we should log it somewhere.
        // Should this happen after loading the configuration?
        if (vm.count("daemonize") > 0) {
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
        boost::interprocess::message_queue pproc_mq{
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

            // Send messages until the parent process sends the terminate signal.
            while (true) {
                constexpr std::string_view msg = "agent factory: still accepting new requests!";
                pproc_mq.send(msg.data(), msg.size(), 0);
                std::this_thread::sleep_for(std::chrono::seconds{3});
            }

            return 0;
        }
        else if (-1 == pid_af) {
            fmt::print(stderr, "Error: could not fork agent factory.\n");
            return 1;
        }
        fmt::print("agent factory pid = [{}]\n", pid_af);

        // Fork delay server if this server is the leader.
        auto pid_ds = fork();
        if (0 == pid_ds) {
            // TODO Set up signal handlers.
            // Only the parent should be able to trigger a config reload via SIGHUP.
            // That means each child process must configure a message queue.
            // Or, we learn how to use zeromq for broadcasting instructions. That is all fine and dandy,
            // but it feels good to be able to select which children should be updated, etc.

            // Send messages until the parent process sends the terminate signal.
            while (true) {
                constexpr std::string_view msg = "delay server: still processing delay rules!";
                pproc_mq.send(msg.data(), msg.size(), 0);
                std::this_thread::sleep_for(std::chrono::seconds{3});
            }

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
            // TODO Set up signal handlers.
            // Only the parent should be able to trigger a config reload via SIGHUP.
            // That means each child process must configure a message queue.
            // Or, we learn how to use zeromq for broadcasting instructions. That is all fine and dandy,
            // but it feels good to be able to select which children should be updated, etc.

            using boost::asio::ip::tcp;

            bool accepting_messages = true;

            boost::asio::io_context io_context;

            tcp::endpoint endpoint{tcp::v4(), 9000}; // This is 1248 is iRODS 5.0.
            tcp::acceptor acceptor{io_context, endpoint};

            while (true) {
                if (!accepting_messages) {
                    std::this_thread::sleep_for(std::chrono::seconds{1});
                    continue;
                }

                tcp::iostream stream;

                boost::system::error_code ec;
                acceptor.accept(stream.socket(), ec);

                if (ec) {
                    fmt::print("Error: control plane: accept: {}\n", ec.message());
                    continue;
                }

                std::string line;
                if (std::getline(stream, line)) {
                    fmt::print("control plane: received: [{}]\n", line);
                }

                if (line == "shutdown") {
                    accepting_messages = false;
                    pproc_mq.send(line.data(), line.size(), 0);
                }
            }

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

        // Enter parent process main loop.
        // 
        // This process should never introduce threads. Everything it cares about must be handled
        // within the loop. This keeps things simple and straight forward.
        //
        // THE PARENT PROCESS IS THE ONLY PROCESS THAT SHOULD/CAN REACT TO SIGNALS!
        // EVERYTHING IS PROPAGATED THROUGH/FROM THE PARENT PROCESS!

        std::array<char, max_msg_size> msg_buf;
        boost::interprocess::message_queue::size_type recvd_size;
        unsigned int priority;

        while (true) {
            msg_buf.fill(0);
            pproc_mq.receive(msg_buf.data(), msg_buf.size(), recvd_size, priority);

            std::string_view msg(msg_buf.data(), recvd_size);
            fmt::print("irodsd: received message: [{}], recvd_size: [{}]\n", msg, recvd_size);

            if (msg == "shutdown") {
                fmt::print("Received shutdown instruction from control plane.\n");
                break;
            }
        }

        kill(pid_cp, SIGTERM);
        kill(pid_af, SIGTERM);
        kill(pid_ds, SIGTERM);

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

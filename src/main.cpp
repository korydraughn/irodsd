#include <irods/rodsClient.h> // For load_client_api_plugins()
#include <irods/client_connection.hpp>
#include <irods/filesystem.hpp>
#include <irods/irods_exception.hpp>

#include <fmt/format.h>

int main(int _argc, char* _argv[]) // NOLINT(modernize-use-trailing-return-type)
{
	if (_argc != 2) {
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
		fmt::print("usage: {} LOGICAL_PATH\n", _argv[0]);
		return 1;
	}

	load_client_api_plugins();

	namespace fs = irods::experimental::filesystem;

	try {
		irods::experimental::client_connection conn;

		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
		for (const auto& e : fs::client::collection_iterator{conn, _argv[1]}) {
			fmt::print("{}\n", e.path().c_str());
		}

		return 0;
	}
	catch (const fs::filesystem_error& e) {
		fmt::print(stderr, "error: {}\n", e.what());
	}
	catch (const irods::exception& e) {
		fmt::print(stderr, "error: {}\n", e.client_display_what());
	}
	catch (const std::exception& e) {
		fmt::print(stderr, "error: {}\n", e.what());
	}

	return 1;
}

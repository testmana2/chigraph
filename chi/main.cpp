#include <chi/Context.hpp>
#include <chi/GraphFunction.hpp>
#include <chi/GraphModule.hpp>
#include <chi/LangModule.hpp>
#include <chi/NodeType.hpp>
#include <chi/Support/Result.hpp>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <llvm/Support/raw_os_ostream.h>

#include <iostream>
#include <string>
#include <vector>

extern int compile(const std::vector<std::string>& opts);
extern int get(const std::vector<std::string>& opts);
extern int run(const std::vector<std::string>& opts, const char* argv0);
extern int interpret(const std::vector<std::string>& opts, const char* argv0);

const char* helpString =
    "Usage: chi [ -C <path> ] <command> <command arguments>\n"
    "\n"
    "Available commands:\n"
    "\n"
    "compile      Compile a chigraph module to an LLVM module\n"
    "run          Run a chigraph module\n"
    "interpret    Interpret LLVM IR (similar to lli)\n"
    "get          Fetch modules from the internet\n"
    "\n"
    "Use chi <command> --help to get usage for a command";

using namespace chi;

int main(int argc, char** argv) {
	namespace po = boost::program_options;

	po::options_description general("chi: Chigraph command line. Usage: chi <command> <arguments>",
	                                50);

	// clang-format off
	general.add_options()
		("change-dir,C", po::value<std::string>(), "Directory to change to first")
		("command", po::value<std::string>(), "which command")
		("subargs", po::value<std::vector<std::string>>(), "arguments for command")
		;
	// clang-format on

	po::positional_options_description pos;
	pos.add("command", 1).add("subargs", -1);

	po::variables_map vm;

	po::parsed_options parsed = po::command_line_parser(argc, argv)
	                                .options(general)
	                                .positional(pos)
	                                .allow_unregistered()
	                                .run();

	po::store(parsed, vm);

	po::notify(vm);

	if (vm.count("C") == 1) { boost::filesystem::current_path(vm["C"].as<std::string>()); }

	if (vm.count("command") != 1) {
		std::cout << helpString << std::endl;
		return 1;
	}

	std::string cmd = vm["command"].as<std::string>();

	std::vector<std::string> opts =
	    po::collect_unrecognized(parsed.options, po::include_positional);
	opts.erase(opts.begin());  // remove the command

	if (cmd == "compile") { return compile(opts); }
	if (cmd == "run") { return run(opts, argv[0]); }
	if (cmd == "interpret") { return interpret(opts, argv[0]); }
	if (cmd == "get") { return get(opts); }
	// TODO: write other ones

	std::cerr << "Unrecognized command: " << cmd << std::endl;
	return 1;
}

#include "UFTTCommandLine.h"

#include "Platform.h"
#include "util/StrFormat.h"

namespace {
	CommandLineCommand getCommandLineCommandFinal(int numargs, const std::vector<std::string>& args, size_t* i)
	{
		int gotargs = args.size() - *i - 1;
		if (gotargs != numargs)
			throw std::runtime_error(STRFORMAT("Wrong number of arguments for %s, got %s, expected %s", args[*i], gotargs, numargs));
		CommandLineCommand ret(args.begin()+*i, args.begin()+*i+1+numargs);
		*i += numargs;
		return ret;
	}

	CommandLineCommand getCommandLineCommand(int numargs, const std::vector<std::string>& args, size_t* i)
	{
		int gotargs = args.size() - *i - 1;
		if (numargs == -1) numargs = gotargs;
		if (gotargs < numargs)
			throw std::runtime_error(STRFORMAT("Wrong number of arguments for %s, got %s, expected %s", args[*i], gotargs, numargs));
		CommandLineCommand ret(args.begin()+*i, args.begin()+*i+1+numargs);
		*i += numargs;
		return ret;
	}

	void validateFilename(std::string& file)
	{
		ext::filesystem::path path(boost::filesystem::system_complete(ext::filesystem::path(file)));
		if (!ext::filesystem::exists(path))
			throw std::runtime_error(STRFORMAT("File not found: %s (%s)", path, file));
		file = path.string();
	}

	template <typename ITER>
	void validateFilenames(ITER begin, ITER end)
	{
		for (; begin != end; ++begin) {
			validateFilename(*begin);
		}
	}
}

CommandLineInfo::CommandLineInfo()
{
	quit = false;
}

CommandLineInfo CommandLineInfo::parseCommandLine(int argc, char** argv)
{
	CommandLineInfo cli;
	std::vector<std::string> args = platform::getUTF8CommandLine(argc, argv);
	std::vector<std::string> sharefiles;
	sharefiles.push_back("--add-shares");
	std::string badfile;

	{
		CommandLineCommand cmd;
		cmd.push_back("--gui-opt");
		cmd.push_back(args[0]);
		cli.list2.push_back(cmd);
	}

	for (size_t i = 1; i < args.size(); ++i) {
		const std::string& arg = args[i];
		if (arg.empty()) {
			badfile = ".";
			break;
		}
		ext::filesystem::path p(boost::filesystem::system_complete(ext::filesystem::path(arg)));
		if (!ext::filesystem::exists(p)) {
			badfile = arg;
			break;
		};
		sharefiles.push_back(p.string());
	};

	if (badfile.empty()) {
		cli.list5.push_back(sharefiles);
		return cli;
	}

	// it was not all files, try parsing options
	for (size_t i = 1; i < args.size(); ++i) {
		const std::string& arg = args[i];
		if (arg.empty() || arg == "--") {
			continue; // skip nops
		} else if (arg == "--console") {
			CommandLineCommand clc;
			clc.push_back(arg);
			cli.list0.push_back(clc);
		} else if (arg == "--sign") {
			cli.list0.push_back(getCommandLineCommandFinal(5, args, &i));
		} else if (arg == "--write-build-version") {
			cli.list0.push_back(getCommandLineCommandFinal(1, args, &i));
		} else if (arg == "--runtest") {
			cli.list0.push_back(getCommandLineCommandFinal(0, args, &i));
		} else if (arg == "--replace") {
			CommandLineCommand testcmd = getCommandLineCommandFinal(1, args, &i);
			CommandLineCommand cmd;
			cmd.push_back(arg);
			cmd.push_back(args[0]);
			cmd.push_back(testcmd[0]);
			cli.list0.push_back(cmd);
		} else if (arg == "--gui-opt") {
			cli.list2.push_back(getCommandLineCommand(1, args, &i));
		} else if (arg == "--gui-opts") {
			cli.list2.push_back(getCommandLineCommand(-1, args, &i));
		} else if (arg == "--add-extra-build") {
			CommandLineCommand cmd = getCommandLineCommand(2, args, &i);
			validateFilename(cmd[2]);
			cli.list5.push_back(cmd);
		} else if (arg == "--delete") {
			CommandLineCommand cmd = getCommandLineCommand(1, args, &i);
			validateFilename(cmd[1]);
			cli.list5.push_back(cmd);
		} else if (arg == "--add-share") {
			CommandLineCommand cmd = getCommandLineCommand(1, args, &i);
			validateFilename(cmd[1]);
			cli.list5.push_back(cmd);
		} else if (arg == "--add-shares") {
			CommandLineCommand cmd = getCommandLineCommand(-1, args, &i);
			validateFilenames(cmd.begin()+1, cmd.end());
			cli.list5.push_back(cmd);
		} else if (arg == "--download-share") {
			CommandLineCommand cmd = getCommandLineCommand(2, args, &i);
			validateFilename(cmd[2]);
			cli.list5.push_back(cmd);
		} else if (arg == "--notify-socket") {
			CommandLineCommand cmd = getCommandLineCommand(1, args, &i);
			cli.list5.push_back(cmd);
		} else if (arg == "--quit") {
			cli.list5.push_back(getCommandLineCommandFinal(0, args, &i));
		} else {
			throw std::runtime_error(std::string()
				+ "Unknown option: " + arg  + "\n"
				+ "(Invalid file: " + badfile + ")\n"
			);
		}
	};
	return cli;
}

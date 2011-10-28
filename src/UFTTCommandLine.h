#ifndef UFTT_COMMAND_LINE_H
#define UFTT_COMMAND_LINE_H

#include <vector>
#include <string>

typedef std::vector<std::string> CommandLineCommand;
typedef std::vector<CommandLineCommand> CommandLineList;
struct CommandLineInfo {
	CommandLineList list0; // direct commands, executed as soon as parsing is done
	CommandLineList list2; // gui options, used to fake argc/argv for gui use
	CommandLineList list5; // async commands, 'started' just before gui is launched, or transferred to previously running uftt instance

	bool quit; // Special list5 command that needs some special care

	CommandLineInfo();

	static CommandLineInfo parseCommandLine(int argc, char** argv);
};

#endif//UFTT_COMMAND_LINE_H

#include <iostream>
#include <cstring>
#include <fstream>
#include <charconv>
#include <functional>
#include <algorithm>

#define NEXT_ARG_VARIABLE(argName, detailVariable) \
	if (!strcmp(arg, argName)) { \
		if (argc < i + 2) { \
			PrintHelp(); \
			return false; \
		} \
		\
		pDetails->detailVariable = nextArg; \
	}

enum class HeaderGuard
{
	NONE = 0,
	PRAGMA = 1,
	IFNDEF = 2,
};

struct CppDetails
{
	// The path of the file to turn into C++
	std::string sourceFilepath;
	// The path to export the content to.
	std::string outputFilepath;
	std::string dataNamespace;
	std::string constantName;
	std::string headerGuardName;

	HeaderGuard guardMode = HeaderGuard::NONE;
};

void PrintHelp()
{
	std::cout << "Usage:\n";
	std::cout << "-s <path> : Sets the source file\n";
	std::cout << "-o <path> : Sets the output file\n";
	std::cout << "-n <namespace> : Sets the namespace of the constant. "
		<< "If this argument is not present, no namespace is set and the constant is a global variable.\n";
	std::cout << "-c <name> : Sets the name of the constant.\n";
	std::cout << "-h <name> : Sets the name of the header guard. -gh must be set for this to work.\n";
	std::cout << "-gp : If this argument is set, a #pragma once will be inserted at the top of the file\n";
	std::cout << "-gh : If this argument is set, header protection will be set at the top of the file.\n";

	std::cout << std::flush;
}

// Processes arguments and then stores the processed information in pDetails.
bool ProcessArguments(uint32_t argc, char* argv[], CppDetails*const pDetails)
{
	// Go through every argument
	for (uint32_t i = 1; i < argc; i++)
	{
		char* arg = argv[i];
		char* nextArg = argv[i + 1];

		NEXT_ARG_VARIABLE("-s", sourceFilepath);
		NEXT_ARG_VARIABLE("-o", outputFilepath);
		NEXT_ARG_VARIABLE("-n", dataNamespace);
		NEXT_ARG_VARIABLE("-c", constantName);
		NEXT_ARG_VARIABLE("-h", headerGuardName);

		if (!strcmp(arg, "-gp"))
			pDetails->guardMode = HeaderGuard::PRAGMA;

		if (!strcmp(arg, "-gh"))
			pDetails->guardMode = HeaderGuard::IFNDEF;
	}

	return true;
}

std::string FilenameToSnakeCase(std::string str)
{
	// Find only the filename of the path
	std::string filename = str.substr(str.find_last_of("/\\") + 1);
	// Then find only the name without the extension
	std::string fileNoExtension = filename.substr(0, filename.find_last_of("."));
	// Then remove special characters
	std::string fileNoSpecial = fileNoExtension;
	{
		std::string special = "`~!@#$%^&**()[]{}\\|;:\'\",<.>/?-_=+";

		for (uint32_t i = 0; i < special.size(); i++)
			fileNoSpecial.erase(std::remove(fileNoSpecial.begin(),
				fileNoSpecial.end(), special[i]), fileNoSpecial.end());
	}

	// THEN replace spaces with underscores
	std::string fileNoSpaces = fileNoSpecial;
	for (uint32_t i = 0; i < fileNoSpaces.size(); i++)
		if (fileNoSpaces[i] == ' ')
			fileNoSpaces[i] = '_';

	return fileNoSpaces;
}

int main(int argc, char* argv[])
{
	// Check if the command has any arguments.

	if (argc < 2)
	{
		PrintHelp();
		return EXIT_FAILURE;
	}

	CppDetails details;
	if (!ProcessArguments(argc, argv, &details))
		return EXIT_FAILURE;

	// The program must have a source and output filepath, 
	// so if the user didn't provide those, exit the program.

	std::ifstream sourceFile(details.sourceFilepath, std::ios::binary | std::ios::ate);
	// Open the output file as an ofstream instead of ifstream
	std::ofstream outputFile(details.outputFilepath);

	// Checks if the source file exists
	if (!sourceFile.is_open())
	{
		std::cout << "Set file doesn't exist or access is denied.\n\n";

		PrintHelp();
		return EXIT_FAILURE;
	}

	std::string output;

	// Header guard
	switch (details.guardMode)
	{
		case HeaderGuard::NONE: break;
		case HeaderGuard::PRAGMA: output += "#pragma once\n\n"; break;
		case HeaderGuard::IFNDEF: 
		{
			std::string headerGuardName = "_";
			if (details.headerGuardName.size() == 0)
			{
				// If the user did not define a custom header guard name,
				// then use the output filename to create one.

				std::string snakeCase = FilenameToSnakeCase(details.outputFilepath);

				// Capitalize the string
				for (uint32_t i = 0; i < snakeCase.size(); i++)
					snakeCase[i] = std::toupper(snakeCase[i]);

				// Now add it to the header guard name
				headerGuardName += snakeCase;
			}
			else
				headerGuardName = details.headerGuardName;

			output += "#ifndef ";
			output += headerGuardName;
			output += "\n#define ";
			output += headerGuardName;
			output += "\n\n";
		}
		break;
	}

	if (details.dataNamespace.size() > 0)
		output += "namespace " + details.dataNamespace + "\n{\n";

	// Constant name
	{
		std::string constantName;
		if (details.constantName.size() == 0)
		{
			// Making the constant name is fairly similar to the header guard,
			// so use the same function.

			std::string snakeCase = FilenameToSnakeCase(details.sourceFilepath);

			// Except make it lowercase
			for (uint32_t i = 0; i < snakeCase.size(); i++)
				snakeCase[i] = std::tolower(snakeCase[i]);

			constantName = "_binary_" + snakeCase;
		}
		else
			constantName = details.constantName;

		if (details.dataNamespace.size() > 0)
			output += "\t";

		output += "const unsigned char " + constantName + "[] = {\n";
	}
	
	{
		// Read every character of the file one by one and stop when EOF or end of file.

		uint64_t size = sourceFile.tellg();
		if (size > 0)
		{
			uint8_t* data = new uint8_t[size];

			// Seek to the beginning of the file and read all data
			sourceFile.seekg(0, std::ios::beg);
			sourceFile.read((char*)data, size);

			// Loop through all file data
			for (uint64_t i = 0; i < size - 1; i++)
			{
				uint8_t byte = data[i];

				char num[4] = {};
				std::to_chars(num, num + 3, byte);

				output += num;
				output += ',';

				if ((i + 1) % 24 == 0)
					output += "\n";
			}

			// Handle the last character differently. Don't add a comma.
			{
				uint8_t byte = data[size - 1];

				char num[4] = {};
				std::to_chars(num, num + 3, byte);

				output += num;
			}
			
			delete[] data;
		}
	}

	output += "\n";
	if (details.dataNamespace.size() > 0)
		output += "\t";
	output += "};\n";

	if (details.dataNamespace.size() > 0)
		output += "}";

	if (details.guardMode == HeaderGuard::IFNDEF)
		output += "\n#endif\n";

	// Write the output to the output file.
	outputFile << output;

	sourceFile.close();
	outputFile.close();

	return EXIT_SUCCESS;
}
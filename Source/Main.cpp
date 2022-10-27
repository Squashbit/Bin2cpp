#include <iostream>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <charconv>
#include <functional>
#include <algorithm>
#include <vector>

#define NEXT_ARG_VARIABLE(argName, detailVariable) \
	if (!strcmp(arg, argName)) { \
		if (argc < i + 2) { \
			PrintHelp(); \
			return false; \
		} \
		\
		pDetails->detailVariable = nextArg; \
		continue; \
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
	bool makeOutputDirectory = false;

	HeaderGuard guardMode = HeaderGuard::NONE;
};

void PrintHelp()
{
	std::cout << 
R"(
Usage:
-s <path>      : Sets the source file
-o <path>      : Sets the output file
-n <namespace> : Sets the namespace of the constant.
-c <name>      : Sets the name of the constant.
-h <name>      : Sets the name of the header guard. -gh must be set for this to work.
-gp            : If this argument is set, a #pragma once will be inserted at the top of the file
-gh            : If this argument is set, header protection will be set at the top of the file.
-mkd           : If this argument is set, the directory that the output file is set to be in will be created automatically.
)"
	<< std::endl;
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

		if (!strcmp(arg, "-mkd")) pDetails->makeOutputDirectory = true; continue;
		if (!strcmp(arg, "-gp")) pDetails->guardMode = HeaderGuard::PRAGMA; continue;
		if (!strcmp(arg, "-gh")) pDetails->guardMode = HeaderGuard::IFNDEF; continue;
	}

	return true;
}

struct PathInfo
{
	// All filename parts (minus drive letter on Windows)
	std::vector<std::string> splitPath;
	// Only works if the path is to a file, not a directory.
	std::vector<std::string> directories;
	// Only works if the path is to a file, not a directory.
	std::string fullFilename;
	// Only works if the path is to a file, not a directory.
	std::string pathNoFilename;
	// The full file path
	std::string fullPath;
	// The path without the drive number
	std::string pathNoDrive;
	// Only works on Windows.
	char driveLetter = NULL;
};

size_t TrimWindowsDriveLetter(PathInfo* pInfo)
{
	size_t readStartIndex = 0;
	if (pInfo->fullPath.size() >= 3)
	{
		if (pInfo->fullPath[1] == ':')
		{
			readStartIndex = 3;
			pInfo->driveLetter = pInfo->fullPath[0];
		}
	}

	return readStartIndex;
}

size_t SplitPathBySlashes(PathInfo* pInfo, size_t driveEndIndex)
{
	size_t lastSplitIndex = driveEndIndex;
	for (size_t i = driveEndIndex; i < pInfo->fullPath.size(); i++)
	{
		if (pInfo->fullPath[i] == '/' || pInfo->fullPath[i] == '\\')
		{
			if (i - lastSplitIndex == 0)
			{
				// If there are multiple slashes in a row, ignore one and go to the
				// next.
				lastSplitIndex = i + 1;
				continue;
			}

			// String with every character since the last split index
			std::string part = pInfo->fullPath.substr(lastSplitIndex, 
				i - lastSplitIndex);

			pInfo->splitPath.push_back(part);
			lastSplitIndex = i + 1;
		}
	}

	// Split once more for filename
	size_t charsSinceSplit = pInfo->fullPath.size() - lastSplitIndex;
	if (charsSinceSplit != 0)
		pInfo->splitPath.push_back(pInfo->fullPath.substr(lastSplitIndex, 
			charsSinceSplit));

	return lastSplitIndex;
}

void ParsePath(PathInfo* pInfo, const std::string& path)
{
	pInfo->fullPath = path;

	// If the path doesn't have a drive letter in it, 
	// this will just be 0 (the start of the string).
	size_t driveLetterEndIndex = TrimWindowsDriveLetter(pInfo);

	size_t filenameStartIndex = SplitPathBySlashes(pInfo, driveLetterEndIndex);

	// Add every split except the last. The directories vector includes all
	// directories, but NOT the filename.
	for (size_t i = 0; i < pInfo->splitPath.size() - 1; i++)
		pInfo->directories.push_back(pInfo->splitPath[i]);

	if (pInfo->splitPath.size() > 0)
		pInfo->fullFilename = pInfo->splitPath[pInfo->splitPath.size() - 1];

	pInfo->pathNoDrive = path.substr(driveLetterEndIndex, 
		path.size() - driveLetterEndIndex);
	pInfo->pathNoFilename = path.substr(0, filenameStartIndex);
}

std::string FilenameToSnakeCase(std::string str)
{
	PathInfo info;
	ParsePath(&info, str);

	std::string fileNoSpecial = info.fullFilename;
	{
		std::string special = "`~!@#$%^&**()[]{}\\|;:\'\",<>/?-_=+";

		for (uint32_t i = 0; i < special.size(); i++)
			fileNoSpecial.erase(std::remove(fileNoSpecial.begin(),
				fileNoSpecial.end(), special[i]), fileNoSpecial.end());
	}

	// THEN replace spaces with underscores
	std::string fileNoSpaces = fileNoSpecial;
	for (uint32_t i = 0; i < fileNoSpaces.size(); i++)
		if (fileNoSpaces[i] == ' ')
			fileNoSpaces[i] = '_';

	// THEN replace dots with underscores
	std::string fileNoDots = fileNoSpaces;
	for (uint32_t i = 0; i < fileNoDots.size(); i++)
		if (fileNoDots[i] == '.')
			fileNoDots[i] = '_';

	return fileNoDots;
}

void AddHeaderGuard(CppDetails& details, std::string& output)
{
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
}

void AddNamespace(CppDetails& details, std::string& output)
{
	if (details.dataNamespace.size() > 0)
		output += "namespace " + details.dataNamespace + "\n{\n";
}

void AddConstantName(CppDetails& details, std::string& output)
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

std::string ReadFileAsNumbers(std::ifstream& sourceFile)
{
	std::string numbers;

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

			numbers += num;
			numbers += ',';

			if ((i + 1) % 24 == 0)
				numbers += "\n";
		}

		// Handle the last character differently. Don't add a comma.
		{
			uint8_t byte = data[size - 1];

			char num[4] = {};
			std::to_chars(num, num + 3, byte);

			numbers += num;
		}

		delete[] data;
	}

	return numbers;
}

int main(int argc, char* argv[])
{
	// Check if the command has any arguments other than the call to the executable.
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
	if (details.sourceFilepath.size() == 0 || details.outputFilepath.size() == 0)
	{
		std::cout << "Provide a source file with -s and an output with -o.\n\n";

		PrintHelp();
		return EXIT_FAILURE;
	}

	std::ifstream sourceFile(details.sourceFilepath, std::ios::binary | std::ios::ate);

	if (!sourceFile.is_open())
	{
		std::cout << "Source file doesn't exist or access is denied.\n\n";

		sourceFile.close();
		
		PrintHelp();
		return EXIT_FAILURE;
	}
	
	if (details.makeOutputDirectory)
	{
		PathInfo info;
		ParsePath(&info, details.outputFilepath);

		std::filesystem::create_directory(info.pathNoFilename);
	}

	// Open the output file as an ofstream instead of ifstream
	std::ofstream outputFile(details.outputFilepath);

	std::string output;
	AddHeaderGuard(details, output);
	AddNamespace(details, output);
	AddConstantName(details, output);

	output += ReadFileAsNumbers(sourceFile);

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
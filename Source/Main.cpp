#include <iostream>
#include <fstream>
#include <charconv>

struct CppDetails
{
	// The path of the file to turn into C++
	std::string sourceFilepath;
	// The path to export the content to.
	std::string outputFilepath;
};

void PrintHelp()
{
	std::cout << "Usage:\n";
	std::cout << "-s <path> : Sets the source file\n";
	std::cout << "-o <path> : Sets the output file\n";
	std::cout << "-n <namespace> : Sets the namespace of the constant. "
		<< "If this argument is not present, no namespace is set and the constant is a global variable.\n";
	std::cout << "-p : If this argument is set, a #pragma once will be inserted at the top of the file\n";
	std::cout << "-h : If this argument is set, header protection will be set at the top of the file.\n";
	std::cout << "-c <name> : Sets the name of the constant.\n";

	std::cout << std::flush;
}

// Processes arguments and then stores the processed information in pDetails.
bool ProcessArguments(uint32_t argc, char* argv[], CppDetails*const pDetails)
{
	// Go through every argument
	for (uint32_t i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-s"))
		{
			// + 2 because one is for the index conversion to size
			// and another to make sure that the user has specified
			// another argument.
			if (argc < i + 2)
			{
				PrintHelp();
				return false;
			}

			pDetails->sourceFilepath = argv[i + 1];
		}

		if (!strcmp(argv[i], "-o"))
		{
			if (argc < i + 2)
			{
				PrintHelp();
				return false;
			}

			pDetails->outputFilepath = argv[i + 1];
		}
	}

	return true;
}

int main(uint32_t argc, char* argv[])
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
	output += "const unsigned char thing[] = {\n\t";
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
					output += "\n\t";
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
	output += "\n};\n";

	// Write the output to the output file.
	outputFile << output;

	sourceFile.close();
	outputFile.close();

	return EXIT_SUCCESS;
}
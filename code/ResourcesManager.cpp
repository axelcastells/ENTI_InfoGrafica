#include <sstream>
#include <iostream>
#include <fstream>

namespace ResourcesManager {
	std::string ReadFile(const char* filePath) {
		std::ifstream shaderFile;
		try
		{
			// open files
			shaderFile.open(filePath);
			std::stringstream shaderStream;
			// read file's buffer contents into streams
			shaderStream << shaderFile.rdbuf();
			// close file handlers
			shaderFile.close();
			// convert stream into string
			return shaderStream.str();
		}
		catch (std::ifstream::failure e)
		{
			std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
		}
	}
}


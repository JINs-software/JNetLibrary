#include "ProtocolSetter.h"
#include <string>
#include <unordered_map>
#include <regex>

#include <Windows.h>

std::map<string, string> g_FilePath;
bool LoadFromFile(const std::string& filePath);
std::string GetPath(const std::string& key);

int main() {
    std::string iniFilePath;

    if (IsDebuggerPresent()) {
#if defined(_DEBUG)
        iniFilePath = "..\\bin\\debug\\ProtocolFile.ini";
#else
        iniFilePath = "..\\bin\\release\\ProtocolFile.ini";
#endif
    }
    else {
        iniFilePath = ".\\ProtocolFile.ini";
    }


    if (!LoadFromFile(iniFilePath)) {
        std::cerr << "Failed to load INI file." << std::endl;
        return 1;
    }
    std::string jsonFilePath = GetPath("json");
    std::string csFilePath = GetPath("c#");
    std::string cppFilePath = GetPath("c++");

    cout << jsonFilePath << endl;
    cout << csFilePath << endl;
    cout << cppFilePath << endl;

	ProtocolSetter setter;
	//if (setter.Init("C:\\Users\\HurJin\\0. GameServerStudy\\1. Project\\MarchOfWindServer\\MarchOfWindServer\\Protocol.json")) {
	//	setter.JsonToCSharp("C:\\Users\\HurJin\\Unity\\MarchOfThWind\\Assets\\Script\\DataFormat\\Protocol.cs");
	//	setter.JsonToCPP("C:\\Users\\HurJin\\0. GameServerStudy\\1. Project\\MarchOfWindServer\\MarchOfWindServer\\Protocol.h");
	//}
    // => �ϵ� �ڵ��� �ƴ� ���
    if (setter.Init(jsonFilePath)) {
        setter.JsonToCSharp(csFilePath);
        setter.JsonToCPP(cppFilePath);
    }
}

bool LoadFromFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open INI file: " << filePath << std::endl;
        return false;
    }

    std::regex pattern("\\[(.*?)\\]\\s(.+)");

    std::string line;
    while (std::getline(file, line)) {
        
        if (!line.empty()) {

            std::smatch match;
            if (std::regex_match(line, match, pattern)) {
                // ��Ī�� ��� ����
                std::string identifier = match[1].str();
                std::string file_path = match[2].str();
                size_t pos = 0;
                while ((pos = file_path.find('\\', pos)) != std::string::npos) {
                    file_path.replace(pos, 1, "\\\\"); // �ϳ��� �齽���ø� �� ���� ��ü
                    pos += 2; // ��ü�� ���� ���ķ� ��� �˻�
                }

                g_FilePath.insert({ identifier, file_path });
            }
            else {
                std::cout << "���ڿ� ������ �߸��Ǿ����ϴ�." << std::endl;
            }
        }
    }

    file.close();
    return true;
}

std::string GetPath(const std::string& key) {
    return g_FilePath[key];
}


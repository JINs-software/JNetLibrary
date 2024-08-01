#pragma once

#include "iostream"
#include "fstream"

#include <map>
#include <list>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h""

using namespace std;
 
class ProtocolSetter
{
	std::map<string, map<string, string>> m_DataTypeMap;
	std::list<pair<string, string>> m_MessageFormatMap;
	std::list<pair<string, list<pair<string, int>>>> m_Constants;
	
	struct Enum {
		string name;
		list<string> fields;
	};
	list<Enum> m_EnumList;

	struct MessageField {
		string type;
		string arrayLenConstGroup = ""; // ""은 단일 자료형을 뜻함
		string arrayLenConst = ""; // ""은 단일 자료형을 뜻함
		string name;
	};
	struct Message {
		string name;
		list<MessageField> fields;
	};
	list<Message> m_MessageList;

private:
	rapidjson::Document m_JsonDoc;

public:
	bool Init(std::string jsonPath) {
		if (LoadJsonFile(jsonPath)) {
			ParseJsonFile();
		}
		else {
			return false;
		}

		for (auto format : m_MessageFormatMap) {
			string type = format.first;
			string name = format.second;

			for (auto& msg : m_MessageList) {
				msg.fields.push_front({ type, "", "", name});
			}
		}

		return true;
	}
	bool JsonToCSharp(std::string csharpFile) {
		std::ofstream outputFile(csharpFile);
		if (!outputFile.is_open()) {
			std::cerr << "Error: Could not open file for writing: " << csharpFile << std::endl;
			return false;
		}

		outputFile << "using System;" << endl;
		outputFile << "using System.Runtime.InteropServices;" << endl << endl;

		// 상수 입력
		PrintConstant(outputFile, "C#");

		// enum 입력
		PrintEnum(outputFile, "C#");
		
		// 메시지 입력
		PrintMessages(outputFile, "C#");

		outputFile.close();
		return true;
	}

	bool JsonToCPP(std::string cppFile) {
		std::ofstream outputFile(cppFile);
		if (!outputFile.is_open()) {
			std::cerr << "Error: Could not open file for writing: " << cppFile << std::endl;
			return false;
		}

		outputFile << "#pragma once" << endl;
		outputFile << "#include <minwindef.h>" << endl << endl;

		// 상수 입력
		PrintConstant(outputFile, "C++");

		// enum 입력
		PrintEnum(outputFile, "C++");

		// 메시지 입력
		PrintMessages(outputFile, "C++");

		outputFile.close();
		return true;
	}

private:
	void PrintConstant(std::ofstream& outputFile, string langType) {
		for (auto constantGroup : m_Constants) {
			string constantGroupName = constantGroup.first;
			list<pair<string, int>>& constantMember = constantGroup.second;

			if (langType == "C#") {
				outputFile << "static class " << constantGroupName << endl;
			}
			else if (langType == "C++") {
				outputFile << "struct " << constantGroupName << endl;
			}

			outputFile << "{" << endl;
			for(auto member : constantMember) 
			{
				if (langType == "C#") {
					outputFile << "\t" << "public const int " << member.first << " = " << member.second << ";" << endl;
				}
				else if (langType == "C++") {
					outputFile << "\t" << "static const int " << member.first << " = " << member.second << ";" << endl;
				}
			}
			outputFile << "};" << endl;
			outputFile << endl;
		}
	}

	void PrintEnum(std::ofstream& outputFile, string langType) {
		//struct Enum {
		//	string name;
		//	list<string> fields;
		//};
		//list<Enum> m_EnumList;

		for (auto Enum : m_EnumList) {
			if (langType == "C#") {
				outputFile << "public " << endl;
			}
			outputFile << "enum " << Enum.name << endl;
			outputFile << "{" << endl;
			for (auto enumItem : Enum.fields) {
				outputFile << "\t" << enumItem << "," << endl;
			}
			outputFile << "};" << endl;
			outputFile << endl;
		}
	}

	void PrintMessages(std::ofstream& outputFile, string langType) {
		
		if (langType == "C++") {
			outputFile << "#pragma pack(push, 1)" << endl << endl;
		}

		for (auto message : m_MessageList) {
			if (langType == "C#") {
				outputFile << "[StructLayout(LayoutKind.Sequential, Pack = 1)]" << endl;
				outputFile << "public class " << message.name << endl;
			}
			else if(langType == "C++") {
				outputFile << "struct " << message.name << endl;
			}

			outputFile << "{" << endl;
			for(auto field : message.fields)
			{
				string type = field.type;
				if (m_DataTypeMap.find(type) != m_DataTypeMap.end()) {
					if (m_DataTypeMap[type].find(langType) != m_DataTypeMap[type].end()) {
						type = m_DataTypeMap[type][langType];
					}
				}
				
				if (langType == "C#") {
					if (field.arrayLenConst == "") {
						outputFile << "\t" << "public " << type << " " << field.name << ";" << endl;
					}
					else {
						outputFile << "\t" << "[MarshalAs(UnmanagedType.ByValArray, SizeConst = " << field.arrayLenConstGroup << "." << field.arrayLenConst << ")]" << endl;
						outputFile << "\t" << "public " << type << "[] " << field.name << ";" << endl;
					}
				}
				else if (langType == "C++") {
					if (field.arrayLenConst == "") {
						outputFile << "\t" << type << " " << field.name << ";" << endl;
					}
					else {
						outputFile << "\t" << type << " " << field.name << "[" << field.arrayLenConstGroup << "::" << field.arrayLenConst << "];" << endl;
					}
				}
			}
			outputFile << "};" << endl;
			outputFile << endl;
		}

		if (langType == "C++") {
			outputFile << "#pragma pack(pop)" << endl;
		}
	}
public:
	bool LoadJsonFile(std::string filePath);
	void ParseJsonFile();

};


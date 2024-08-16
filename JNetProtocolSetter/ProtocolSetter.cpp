#include "ProtocolSetter.h"
#include <Windows.h>
#include "sstream"
using namespace rapidjson;

bool ProtocolSetter::LoadJsonFile(string filePath)
{
	ifstream file(filePath);
	string fileStr;

	if (!file.is_open()) {
		cerr << "Error: Could not open file, errno: " << errno << endl;
		perror("Error details");
		return false;
	}
	else {
		std::stringstream buffer;
		buffer << file.rdbuf();
		std::string fileStr = buffer.str();

		m_JsonDoc.Parse(const_cast<char*>(fileStr.c_str()));

		if (m_JsonDoc.HasParseError()) {
			std::cerr << "JSON parse error: " << m_JsonDoc.GetParseError() << std::endl;
			return false;
		}
	}
	return true;
}

void ProtocolSetter::ParseJsonFile()
{
	for (auto member = m_JsonDoc.MemberBegin(); member != m_JsonDoc.MemberEnd(); ++member) {
		std::cout << "Key: " << member->name.GetString() << std::endl;

		Value& object = member->value;
		for (auto item = object.MemberBegin(); item != object.MemberEnd(); item++) {
			string itemName = item->name.GetString();
			if (itemName == "type") {
				string type = item->value.GetString();
				
				if (type == "dataTypeMapping") {
					if (object.HasMember("mapping")) {
						const auto& mappings = object["mapping"];
						auto mapArr = mappings.GetArray();
						for (auto iter = mapArr.Begin(); iter != mapArr.End(); iter++) {
							const Value& mapping = *iter;

							string dataType;
							if(mapping.HasMember("dataType")) {
								//cout << mapping["dataType"].GetString() << endl;
								dataType = mapping["dataType"].GetString();
								m_DataTypeMap.insert({ dataType, {} });
							}
							else {
								// 데이터 타입 없음
								DebugBreak();
							}

							if (mapping.HasMember("C#")) {
								//cout << mapping["C#"].GetString() << endl;
								m_DataTypeMap[dataType].insert({ "C#", mapping["C#"].GetString() });
							}
							if (mapping.HasMember("C++")) {
								//cout << mapping["C#"].GetString() << endl;
								m_DataTypeMap[dataType].insert({ "C++", mapping["C++"].GetString() });
							}
						}
					}
					else {
						DebugBreak();
					}
				}
				else if (type == "messageFormat") {
					if (object.HasMember("field")) {
						auto fieldArr = object["field"].GetArray();
						for (auto arrIter = fieldArr.Begin(); arrIter != fieldArr.End(); arrIter++) {
							const auto& val = *arrIter;
							if (val.HasMember("type") && val.HasMember("name")) {
								m_MessageFormatMap.push_back({ val["type"].GetString(), val["name"].GetString() });
							}
							else {
								DebugBreak();
							}
						}
					}
					else {
						DebugBreak();
					}
				}
				else if (type == "include") {
					if (object.HasMember("references")) {
						const auto& references = object["references"];
						auto refArr = references.GetArray();
						for (auto iter = refArr.Begin(); iter != refArr.End(); iter++) {
							const Value& ref = *iter;

							string lang;
							if (ref.HasMember("lang")) {
								lang = ref["lang"].GetString();
								if (lang == "C++") {
									pair<string, bool> hdrInfo;
									hdrInfo.first = ref["header"].GetString();
									hdrInfo.second = ref["system"].GetBool();

									m_CPPHeader.push_back(hdrInfo);
								}
								else if (lang == "C#") {
									// .. to do
								}
								else {
									DebugBreak();
								}
							}
							else {
								DebugBreak();
							}
						}
					}
					else {
						DebugBreak();
					}
				}
				else if (type == "constant") {
					string constGroup;

					list<pair<string, int>> constList;
					if (object.HasMember("group") && object.HasMember("constant")) {
						constGroup = object["group"].GetString();

						auto fieldArr = object["constant"].GetArray();
						for (auto arrIter = fieldArr.Begin(); arrIter != fieldArr.End(); arrIter++) {
							if (arrIter->HasMember("name") && arrIter->HasMember("value")) {
								string constName = (*arrIter)["name"].GetString();
								int constValue = (*arrIter)["value"].GetInt();
								constList.push_back({ constName, constValue });
							}
							else {
								DebugBreak();
							}
						}

						m_Constants.push_back({ constGroup, constList });
					}
					else {
						DebugBreak();
					}
				}
				else if (type == "enum") {
					string enumName;
					list<string> enumItemList;

					if (object.HasMember("name") && object.HasMember("field")) {
						enumName = object["name"].GetString();
					
						auto fieldArr = object["field"].GetArray();
						for (auto arrIter = fieldArr.Begin(); arrIter != fieldArr.End(); arrIter++) {
							string enumItem = arrIter->GetString();
							enumItemList.push_back(enumItem);
						}
					}
					else {
						DebugBreak();
					}
					
					m_EnumList.push_back({ enumName, enumItemList });

				}
				else if (type == "message") {
					string msgName;
					list<MessageField> memberFieldList;

					if (object.HasMember("name") && object.HasMember("field")) {
						msgName = object["name"].GetString();

						auto fieldArr = object["field"].GetArray();
						for (auto arrIter = fieldArr.Begin(); arrIter != fieldArr.End(); arrIter++) {
							const auto& val = *arrIter;
							if (val.HasMember("type") && val.HasMember("name")) {
								MessageField member;
								member.type = val["type"].GetString();
								member.name = val["name"].GetString();
								if (val.HasMember("array")) {
									//member.arrayLen = val["array"].GetString();
									auto constInfo = val["array"].GetArray();
									member.arrayLenConstGroup = constInfo[0].GetString();
									member.arrayLenConst = constInfo[1].GetString();
								}
								else {
									member.arrayLenConst = "";
								}

								memberFieldList.push_back(member);
							}
							else {
								DebugBreak();
							}
						}

						m_MessageList.push_back({ msgName, memberFieldList });
					}
					else {
						DebugBreak();
					}
				}
				else {
					DebugBreak();
				}
			}
		}
	}

}
;
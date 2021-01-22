#include "UserDefaults.h"
#include "UserInteractions.h"
#include "SurgeStorage.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <map>
#include <sstream>
#include <fstream>

#include "filesystem/import.h"

namespace Surge
{
namespace Storage
{

/*
** These functions and variables are not exported in the header 
*/
struct UserDefaultValue
{
    typedef enum ValueType
    {
        ud_string = 1,
        ud_int = 2
    } ValueType;

    std::string key; 
    std::string value;
    ValueType   type;
};

std::map<std::string, UserDefaultValue> defaultsFileContents;
bool haveReadDefaultsFile = false;

std::string defaultsFileName(SurgeStorage *storage)
{
    std::string fn = storage->userDefaultFilePath + PATH_SEPARATOR + "SurgeUserDefaults.xml";
    return fn;
}

void readDefaultsFile(std::string fn, bool forceRead=false)
{
    if (!haveReadDefaultsFile || forceRead)
    {
        defaultsFileContents.clear();

        TiXmlDocument defaultsLoader;
        defaultsLoader.LoadFile(string_to_path(fn));
        TiXmlElement *e = TINYXML_SAFE_TO_ELEMENT(defaultsLoader.FirstChild("defaults"));
        if(e)
        {
            const char* version = e->Attribute("version");
            if (strcmp(version, "1") != 0)
            {
                std::ostringstream oss;
                oss << "This version of Surge only reads version 1 defaults. You user defaults version is "
                    << version << ". Defaults ignored";
                Surge::UserInteractions::promptError(oss.str(), "File Version Error");
                return;
            }

            TiXmlElement *def = TINYXML_SAFE_TO_ELEMENT(e->FirstChild("default"));
            while(def)
            {
                UserDefaultValue v;
                v.key = def->Attribute("key");
                v.value = def->Attribute("value");
                int vt;
                def->Attribute("type", &vt);
                v.type = (UserDefaultValue::ValueType)vt;

                defaultsFileContents[v.key] = v;

                def = TINYXML_SAFE_TO_ELEMENT(def->NextSibling("default"));
            }
        }
        haveReadDefaultsFile = true;
    }
}

bool storeUserDefaultValue(SurgeStorage *storage, const std::string &key, const std::string &val, UserDefaultValue::ValueType type)
{
    // Re-read the file in case another surge has updated it
    readDefaultsFile(defaultsFileName(storage), true);

    /*
    ** Surge has a habit of creating the user directories it needs. 
    ** See SurgeSytnehsizer::savePatch for instance
    ** and so we have to do the same here
    */
    fs::create_directories(string_to_path(storage->userDefaultFilePath));

    
    UserDefaultValue v;
    v.key = key;
    v.value = val;
    v.type = type;

    defaultsFileContents[v.key] = v;

    /*
    ** For now, the format of our defaults file is so simple that we don't need to mess
    ** around with tinyxml to create it, just to parse it
    */
    std::ofstream dFile(string_to_path(defaultsFileName(storage)));
    if (!dFile.is_open())
    {
        std::ostringstream emsg;
        emsg << "Unable to open defaults file '" << defaultsFileName(storage) << "' for writing.";
        Surge::UserInteractions::promptError(emsg.str(), "Defaults Not Saved");
        return false;
    }
        
    dFile << "<?xml version = \"1.0\" encoding = \"UTF-8\" ?>\n"
          << "<!-- User Defaults for Surge Synthesizer -->\n"
          << "<defaults version=\"1\">" << std::endl;

    for (auto &el : defaultsFileContents)
    {
        dFile << "  <default key=\"" << el.first << "\" value=\"" << el.second.value << "\" type=\"" << (int)el.second.type << "\"/>\n";
    }

    dFile << "</defaults>" << std::endl;
    dFile.close();

    return true;
}

/*
** Functions from the header
*/    

std::string getUserDefaultValue(SurgeStorage *storage, const std::string &key, const std::string &valueIfMissing)
{
    readDefaultsFile(defaultsFileName(storage));

    if (defaultsFileContents.find(key) != defaultsFileContents.end())
    {
        auto vStruct = defaultsFileContents[key];
        if(vStruct.type != UserDefaultValue::ud_string)
        {
            return valueIfMissing;
        }
        return vStruct.value;
    }
   
    return valueIfMissing;
}

int getUserDefaultValue(SurgeStorage *storage, const std::string &key, int valueIfMissing)
{
    readDefaultsFile(defaultsFileName(storage));

    if (defaultsFileContents.find(key) != defaultsFileContents.end())
    {
        auto vStruct = defaultsFileContents[key];
        if(vStruct.type != UserDefaultValue::ud_int)
        {
            return valueIfMissing;
        }
        return std::stoi(vStruct.value);
    }
    return valueIfMissing;
}

bool updateUserDefaultValue(SurgeStorage *storage, const std::string &key, const std::string &value)
{
    return storeUserDefaultValue(storage, key, value, UserDefaultValue::ud_string);
}

bool updateUserDefaultValue(SurgeStorage *storage, const std::string &key, const int value)
{
    std::ostringstream oss;
    oss << value;
    return storeUserDefaultValue(storage, key, oss.str(), UserDefaultValue::ud_int);
}

}
}

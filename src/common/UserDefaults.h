#pragma once

#include <string>
#include <vector>

/*
** Surge has a variety of settings which users can update and save across
** sessions. These two functions give a vector to a persisted key-value
** pair store which we can use in our application logic. Our reference
** to SurgeStorage allows us to find paths in which we can load or
** persist this information.
*/


class SurgeStorage;

namespace Surge
{
namespace Storage
{

/**
 * getUserDefaultValue
 *
 * Given a key, return the std::string which is the persisted user default value.
 * If no such key is persisted, return the value "valueIfMissing". There is a variation
 * on this for both std::string and int stored values.
 */
std::string getUserDefaultValue(SurgeStorage *storage, const std::string &key, const std::string &valueIfMissing);
int         getUserDefaultValue(SurgeStorage *storage, const std::string &key, int valueIfMissing);

/**
 * updateUserDefaultValue
 *
 * Given a key and a value, update the user default file
 */
bool updateUserDefaultValue(SurgeStorage *storage, const std::string &key, const std::string &value);
bool updateUserDefaultValue(SurgeStorage *storage, const std::string &key, const int value);

}
}

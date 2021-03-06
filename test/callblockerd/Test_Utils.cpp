/*
 callblocker - blocking unwanted calls from your home phone
 Copyright (C) 2015-2016 Patrick Ammann <pammann@gmx.net>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 3
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "Test.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "Settings.h"
#include "Utils.h"


static void TestCase_fileSystem()
{
  std::string res = Utils::pathJoin("/etc/callblocker/cache/", "a.json");
  assert(res == "/etc/callblocker/cache/a.json");
  res = Utils::pathJoin("/etc/callblocker/cache", "a.json");
  assert(res == "/etc/callblocker/cache/a.json");

  res = Utils::pathBasename("name.json");
  assert(res == "name.json");
  res = Utils::pathBasename("/etc/callblocker/cache/a.json");
  assert(res == "a.json");
  res = Utils::pathBasename("/etc/callblocker/cache/");
  assert(res == "");

  res = Utils::pathDirname("/etc/callblocker/cache/a.json");
  assert(res == "/etc/callblocker/cache");
  res = Utils::pathDirname("/etc/callblocker/cache/");
  assert(res == "/etc/callblocker/cache");
  res = Utils::pathDirname("a.json");
  assert(res == "");
}

static void TestCase_string()
{
  // startsWith
  assert(Utils::startsWith("+212122112", "+") == true);
  assert(Utils::startsWith("+212122112", "-") == false);

  // trim
  std::string str = "123";
  Utils::trim(&str);
  assert(str.compare("123") == 0);

  str = "  123";
  Utils::trim(&str);
  assert(str.compare("123") == 0);

  str = "123  ";
  Utils::trim(&str);
  assert(str.compare("123") == 0);

  str = "  123  ";
  Utils::trim(&str);
  assert(str.compare("123") == 0);

  str = "\t\n123\n\n\n";
  Utils::trim(&str);
  assert(str.compare("123") == 0);
}

static void TestCase_makeNumberInternational()
{
  SettingBase settingsBase;
  settingsBase.blockAnonymousCID = false;
  settingsBase.blockInvalidCID = false;

  static const struct
  {
    std::string countryCode;
    std::string number;
    std::string expectedNumber;
    bool valid;
  } tests[] =
  {
    // CH
    { "+41", "+41791234567",        "+41791234567",        true  }, // already international (SIP phone case)
    { "+41", "0041791234567",       "+41791234567",        true  }, // already international (Analog phone case)
    { "+41", "0441234567",          "+41441234567",        true  }, // local
    { "+41", "**600",               "**600",               true  }, // intern, not public number
    { "+41", "+4144888",            "+4144888",            false }, // invalid: too short number
    { "+41", "+493456789012345678", "+493456789012345678", false }, // invalid: too long number
    { "+41", "+99986203236",        "+99986203236",        false }, // invalid: unassigned country code
    { "+41", "+219225595",          "+219225595",          false }, // invalid: unassigned country code
    
    // US
    { "+1", "+15403221123",         "+15403221123",        true  }, // already international (SIP phone case)
    { "+1", "15403221123",          "+15403221123",        true  }  // already international (Analog phone case)
  };

  for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
  {
    settingsBase.countryCode = tests[i].countryCode;
    std::string str = tests[i].number;
    bool valid;
    Utils::makeNumberE164(&settingsBase, &str, &valid);
    if (str.compare(tests[i].expectedNumber) != 0)
    {
      printf("found '%s' expected '%s' for '%s'\n", str.c_str(), tests[i].expectedNumber.c_str(), tests[i].number.c_str());
    }
    assert(str.compare(tests[i].expectedNumber) == 0);
    if (valid != tests[i].valid)
    {
      printf("valid different than expected %d for '%s'\n", tests[i].valid, tests[i].number.c_str());
    }
    assert(valid == tests[i].valid);
  }
}

static void TestCase_parseCallerID()
{
  std::vector<std::pair<std::string, std::string>> result;
  std::string str = "DATE=0306\nTIME=1517\nNMBR=0123456789\nNAME=aasd asdd\n";
  Utils::parseCallerID(str, &result);
  assert(result.size() == 4);
  assert(result[0].first.compare("DATE") == 0);
  assert(result[0].second.compare("0306") == 0);
  assert(result[1].first.compare("TIME") == 0);
  assert(result[1].second.compare("1517") == 0);
  assert(result[2].first.compare("NMBR") == 0);
  assert(result[2].second.compare("0123456789") == 0);
  assert(result[3].first.compare("NAME") == 0);
  assert(result[3].second.compare("aasd asdd") == 0);
  result.clear();

  str = "DATE\n TIME=12";
  Utils::parseCallerID(str, &result);
  assert(result.size() == 1);
  assert(result[0].first.compare("TIME") == 0);
  assert(result[0].second.compare("12") == 0);
  result.clear();

  str = "DATE= 12\nTIME=1\nNMBR=\nNAME=saas ";
  Utils::parseCallerID(str, &result);
  assert(result.size() == 4);
  assert(result[0].first.compare("DATE") == 0);
  assert(result[0].second.compare("12") == 0);
  assert(result[1].first.compare("TIME") == 0);
  assert(result[1].second.compare("1") == 0);
  assert(result[2].first.compare("NMBR") == 0);
  assert(result[2].second.compare("") == 0);
  assert(result[3].first.compare("NAME") == 0);
  assert(result[3].second.compare("saas") == 0);
  result.clear();
}

static void TestCase_time()
{
  std::string original = "2015-05-15 12:00:00 +0000";
  std::chrono::system_clock::time_point tp;
  assert(Utils::parseTime(original, &tp));
  std::string reparsed = Utils::formatTime(tp);
  assert(original == reparsed);
}

void Test_Utils_Run()
{
  printf("Test_Utils_Run...\n");
  
  TestCase_fileSystem();
  TestCase_string();
  TestCase_makeNumberInternational();
  TestCase_parseCallerID();
  TestCase_time();
}


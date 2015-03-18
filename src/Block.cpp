/*
 callblocker - blocking unwanted calls from your home phone
 Copyright (C) 2015-2015 Patrick Ammann <pammann@gmx.net>

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

#include "Block.h" // API

#include <json-c/json.h>
#include <boost/algorithm/string/predicate.hpp>

#include "Logger.h"
#include "Helper.h"


Block::Block(Settings* pSettings) {
  Logger::debug("Block...");
  m_pSettings = pSettings;

  m_pWhitelists = new FileLists(SYSCONFDIR "/" PACKAGE_NAME "/whitelists");
  m_pBlacklists = new FileLists(SYSCONFDIR "/" PACKAGE_NAME "/blacklists");
}

Block::~Block() {
  Logger::debug("~Block...");

  delete m_pWhitelists;
  m_pWhitelists = NULL;
  delete m_pBlacklists;
  m_pBlacklists = NULL;
}

void Block::run() {
  m_pWhitelists->run();
  m_pBlacklists->run();
}

bool Block::isNumberBlocked(const struct SettingBase* pSettings, const std::string& rNumber, std::string* pMsg) {
  Logger::debug("Block::isNumberBlocked(settings=%s, number=%s)", pSettings->toString().c_str(), rNumber.c_str());

  std::string reason = "";
  std::string msg = "";
  bool block;
  switch (pSettings->blockMode) {
    default:
      Logger::warn("invalid block mode %d", pSettings->blockMode);
    case LOGGING_ONLY:
      block = false;
      if (isWhiteListed(pSettings, rNumber, &msg)) {
        reason = "would be in whitelist ("+msg+")";
        break;
      }
      if (isBlacklisted(pSettings, rNumber, &msg)) {
        reason = "would be in blacklist ("+msg+")";
        break;
      }
      reason = "would not be in white- and blacklists ("+msg+")";
      break;

    case WHITELISTS_ONLY:
      if (isWhiteListed(pSettings, rNumber, &msg)) {
        reason = "found in whitelist ("+msg+")";
        block = false;
        break;
      }
      reason = "not found in whitelists";
      block = true;
      break;

    case WHITELISTS_AND_BLACKLISTS:
      if (isWhiteListed(pSettings, rNumber, &msg)) {
        reason = "found in whitelist ("+msg+")";
        block = false;
        break;
      }
      if (isBlacklisted(pSettings, rNumber, &msg)) {
        reason = "found in blacklist ("+msg+")";
        block = true;
        break;
      }
      reason = "not found in white- and blacklists ("+msg+")";
      block = false;
      break;

    case BLACKLISTS_ONLY:
      if (isBlacklisted(pSettings, rNumber, &msg)) {
        reason = "found in blacklist ("+msg+")";
        block = true;
        break;
      }
      reason = "not found in blacklists ("+msg+")";
      block = false;
      break;
  }

  std::string res = "Incoming call from ";
  res += rNumber;
  if (block) {
    res += " is blocked";
  }
  if (reason.length() > 0) {
    res += " [";
    res += reason;
    res += "]";
  }
  *pMsg = res;

  return block;
}

bool Block::isWhiteListed(const struct SettingBase* pSettings, const std::string& rNumber, std::string* pMsg) {
  return m_pWhitelists->isListed(rNumber, pMsg);
}

bool Block::isBlacklisted(const struct SettingBase* pSettings, const std::string& rNumber, std::string* pMsg) {
  if (m_pBlacklists->isListed(rNumber, pMsg)) {
    return true;
  }

#if 1
  if (boost::starts_with(rNumber, "**")) {
    // it is an intern number, thus makes no sense to ask the world
    *pMsg = "intern number";
    return false;
  }
#endif

  // online check if spam
  if (pSettings->onlineCheck.length() != 0) {
    struct json_object* root;
    if (checkOnline("onlinecheck_", pSettings->onlineCheck, rNumber, &root)) {
      bool spam;
      if (!Helper::getObject(root, "spam", true, "script result", &spam)) {
        return false;
      }
      if (spam) {
        (void)Helper::getObject(root, "comment", false, "script result", pMsg);
        return true;
      }
    }
  }

  // online lookup caller name
  if (pSettings->onlineLookup.length() != 0) {
    struct json_object* root;
    if (checkOnline("onlinelookup_", pSettings->onlineLookup, rNumber, &root)) {
      (void)Helper::getObject(root, "name", false, "script result", pMsg);
    }
  }

  // no spam
  return false;
}

bool Block::checkOnline(std::string prefix, std::string name, const std::string& rNumber, struct json_object** root) {
    std::string script = "/usr/share/callblocker/" + prefix + name + ".py";

    std::string parameters = "--number " + rNumber;
    std::vector<struct SettingOnlineCredential> creds = m_pSettings->getOnlineCredentials();
    for(size_t i = 0; i < creds.size(); i++) {
      struct SettingOnlineCredential* cred = &creds[i];
      if (cred->name == name) {
        for (std::map<std::string,std::string>::iterator it = cred->data.begin(); it != cred->data.end(); ++it) {
          parameters += " --" + it->first + " " + it->second;
        }
        break;
      }
    }

    std::string res;
    if (!Helper::executeCommand(script + " " + parameters + " 2>&1", &res)) {
      return false;
    }

    *root = json_tokener_parse(res.c_str());
    return true;
}


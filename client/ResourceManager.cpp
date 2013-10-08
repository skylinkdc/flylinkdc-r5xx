/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdinc.h"
#include "ResourceManager.h"
#include "../XMLParser/XMLParser.h"
#include "Text.h"

dcdrun(bool ResourceManager::g_debugStarted = false;) // [+] IRainman fix.

wstring ResourceManager::wstrings[ResourceManager::LAST];

void ResourceManager::loadLanguage(const string& aFile)
{
	XMLParser::XMLResults xRes;
	// Try to parse data
	XMLParser::XMLNode xRootNode = XMLParser::XMLNode::parseFile(Text::toT(aFile).c_str(), 0, &xRes); // [!] IRainman fix.
	
	if (xRes.error == XMLParser::eXMLErrorNone)
	{
		unordered_map<string, int> l_handler;
		
		for (int i = 0; i < LAST; ++i)
		{
			l_handler[names[i]] = i;
		}
		
		XMLParser::XMLNode ResNode = xRootNode.getChildNode("Language");
		if (!ResNode.isEmpty())
		{
			XMLParser::XMLNode StringsNode = ResNode.getChildNode("Strings");
			if (!StringsNode.isEmpty())
			{
				int i = 0;
				XMLParser::XMLNode StringNode = StringsNode.getChildNode("String", &i);
				while (!StringNode.isEmpty())
				{
					const auto j = l_handler.find(StringNode.getAttributeOrDefault("Name"));
					
					if (j != l_handler.end())
					{
						strings[j->second] = StringNode.getTextOrDefault(); // [!] IRainman fix.
						strings[j->second].shrink_to_fit(); // [+]IRainman opt.
					}
					
					StringNode = StringsNode.getChildNode("String", &i);
				}
				createWide();
			}
		}
	}
}

void ResourceManager::createWide()
{
	for (size_t i = 0; i < LAST; ++i)
	{
		wstrings[i].clear();
		if (!strings[i].empty())
		{
			Text::toT(strings[i], wstrings[i]);
			wstrings[i].shrink_to_fit(); // [+]IRainman opt
		}
	}
}

/**
 * @file
 * $Id: ResourceManager.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
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

#ifndef DCPLUSPLUS_DCPP_SEARCH_MANAGER_H
#define DCPLUSPLUS_DCPP_SEARCH_MANAGER_H

#include "Thread.h"
#include "StringSearch.h" // [+] IRainman-S
#include "SearchManagerListener.h"
#include "AdcCommand.h"
#include "ClientManager.h"

class SocketException;

class SearchManager : public Speaker<SearchManagerListener>, public Singleton<SearchManager>, public BASE_THREAD
{
	public:
	
		enum TypeModes
		{
			TYPE_ANY = 0,
			TYPE_AUDIO,
			TYPE_COMPRESSED,
			TYPE_DOCUMENT,
			TYPE_EXECUTABLE,
			TYPE_PICTURE,
			TYPE_VIDEO,
			TYPE_DIRECTORY,
			TYPE_TTH,
			TYPE_CD_IMAGE, //[+] �� flylinkdc++
			TYPE_LAST
		};
	public:
		static const char* getTypeStr(int type);
		
		void search(const string& aName, int64_t aSize, TypeModes aTypeMode, Search::SizeModes aSizeMode, const string& aToken, void* aOwner = nullptr);
		void search(const string& aName, const string& aSize, TypeModes aTypeMode, Search::SizeModes aSizeMode, const string& aToken, void* aOwner = nullptr)
		{
			search(aName, Util::toInt64(aSize), aTypeMode, aSizeMode, aToken, aOwner);
		}
		
		uint64_t search(const StringList& who, const string& aName, int64_t aSize, TypeModes aTypeMode, Search::SizeModes aSizeMode, const string& aToken, const StringList& aExtList, void* aOwner = nullptr);
		uint64_t search(const StringList& who, const string& aName, const string& aSize, TypeModes aTypeMode, Search::SizeModes aSizeMode, const string& aToken, const StringList& aExtList, void* aOwner = nullptr)
		{
			return search(who, aName, Util::toInt64(aSize), aTypeMode, aSizeMode, aToken, aExtList, aOwner);
		}
		//static string clean(const string& aSearchString);
		
		ClientManagerListener::SearchReply respond(const AdcCommand& cmd, const CID& cid, bool isUdpActive, const string& hubIpPort, StringSearch::List& reguest); // [!] IRainman-S add  StringSearch::List& reguest and return type
		
		uint16_t getPort() const
		{
			return port;
		}
		
		void listen();
		void disconnect() noexcept;
		void onSearchResult(const string& aLine)
		{
			onData((const uint8_t*)aLine.data(), aLine.length(), Util::emptyString);
		}
		
		void onRES(const AdcCommand& cmd, const UserPtr& from, const string& remoteIp = Util::emptyString);
		void onPSR(const AdcCommand& cmd, UserPtr from, const string& remoteIp = Util::emptyString);
		AdcCommand toPSR(bool wantResponse, const string& myNick, const string& hubIpPort, const string& tth, const vector<uint16_t>& partialInfo) const;
		
	private:
		class UdpQueue: public BASE_THREAD
		{
			public:
				UdpQueue() : stop(false) {}
				~UdpQueue()
				{
					shutdown();
				}
				
				int run();
				void shutdown()
				{
					stop = true;
					s.signal();
				}
				void addResult(const string& buf, const string& ip)
				{
					{
						FastLock l(cs);
						resultList.push_back(make_pair(buf, ip));
					}
					s.signal();
				} // Venturi Firewall 2012-04-23_22-28-18_A6JRQEPFW5263A7S7ZOBOAJGFCMET3YJCUYOVCQ_34B61CDE_crash-stack-r501-build-9812.dmp
				
			private:
				FastCriticalSection cs; // [!] IRainman opt: use spin lock here.
				Semaphore s;
				
				deque<pair<string, string>> resultList;
				
				volatile bool stop; // [!] IRainman fix: this variable is volatile.
		} m_queue;
		
		// [-] CriticalSection cs; [-] FlylinkDC++
		unique_ptr<Socket> socket;
		uint16_t port;
		volatile bool m_stop; // [!] IRainman fix: this variable is volatile.
		friend class Singleton<SearchManager>;
		
		SearchManager();
		
		static std::string normalizeWhitespace(const std::string& aString);
		int run();
		
		~SearchManager();
		void onData(const uint8_t* buf, size_t aLen, const string& address);
		
		string getPartsString(const PartsInfo& partsInfo) const;
};

#endif // !defined(SEARCH_MANAGER_H)

/**
 * @file
 * $Id: SearchManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
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

#ifndef DCPLUSPLUS_DCPP_USER_H
#define DCPLUSPLUS_DCPP_USER_H

#include "Pointer.h"
#include "CID.h"
#include "Flags.h"
#include "forward.h"
#include "CFlyProfiler.h"

class ClientBase;

#define TAG(x,y) (x + (y << 8)) // TODO static_assert

#include "CFlyUserRatioInfo.h"

/** A user connected to one or more hubs. */
class User : public intrusive_ptr_base<User>, public Flags
{
	public:
		// [!]IRainman This enum is necessary for a comfortable job status flags
		enum Bits
		{
			ONLINE_BIT,
			// [!] IRainman fix.
			// [-] PASSIVE_BIT, deprecated.
			NMDC_FILES_PASSIVE_BIT, // [+]
			NMDC_SEARCH_PASSIVE_BIT, // [+]
			TCP4_BIT = NMDC_FILES_PASSIVE_BIT, // [+]
			UDP4_BIT = NMDC_SEARCH_PASSIVE_BIT, // [+]
			TCP6_BIT, // [+]
			UDP6_BIT, // [+]
			// [~] IRainman fix.
			NMDC_BIT,
#ifdef IRAINMAN_ENABLE_TTH_GET_FLAG
			TTH_GET_BIT, //[+]FlylinkDC
#endif
			TLS_BIT,
#ifndef IRAINMAN_TEMPORARY_DISABLE_XXX_ICON
			GREY_XXX_5_BIT,
			GREY_XXX_6_BIT,
#endif
			AWAY_BIT, //[+]FlylinkDC
			SERVER_BIT,
			FIREBALL_BIT,
			
			NO_ADC_1_0_PROTOCOL_BIT,
			NO_ADCS_0_10_PROTOCOL_BIT,
			DHT_BIT,
			NAT_BIT,
#ifdef PPA_INCLUDE_IPFILTER
			PG_BLOCK_BIT
#endif
		};
		
		/** Each flag is set if it's true in at least one hub */
		enum UserFlags
		{
			ONLINE = 1 << ONLINE_BIT,
			// [!] IRainman fix.
			// [-] PASSIVE = 1 << PASSIVE_BIT, deprecated
			NMDC_FILES_PASSIVE = 1 << NMDC_FILES_PASSIVE_BIT, // [!]
			NMDC_SEARCH_PASSIVE = 1 << NMDC_SEARCH_PASSIVE_BIT, // [+]
			TCP4 = 1 << TCP4_BIT, // [+]
			UDP4 = 1 << UDP4_BIT, // [+]
			TCP6 = 1 << TCP6_BIT, // [+]
			UDP6 = 1 << UDP6_BIT, // [+]
			// [~] IRainman fix.
			NMDC = 1 << NMDC_BIT,
#ifdef IRAINMAN_ENABLE_TTH_GET_FLAG
			TTH_GET = 1 << TTH_GET_BIT,     //< User supports getting files by tth -> don't have path in queue...
#endif
			TLS = 1 << TLS_BIT,             //< Client supports TLS
			ADCS = TLS,                     //< Client supports TLS
#ifndef IRAINMAN_TEMPORARY_DISABLE_XXX_ICON
			GREY_XXX_5 = 1 << GREY_XXX_5_BIT,
			GREY_XXX_6 = 1 << GREY_XXX_6_BIT,
#endif
			AWAY = 1 << AWAY_BIT,
			SERVER = 1 << SERVER_BIT,
			FIREBALL = 1 << FIREBALL_BIT,
			
			NO_ADC_1_0_PROTOCOL = 1 << NO_ADC_1_0_PROTOCOL_BIT,
			NO_ADCS_0_10_PROTOCOL = 1 << NO_ADCS_0_10_PROTOCOL_BIT,
			
			DHT0 = 1 << DHT_BIT,
			NAT0 = 1 << NAT_BIT,
			
#ifdef PPA_INCLUDE_IPFILTER
			PG_BLOCK = 1 << PG_BLOCK_BIT
#endif
		};
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		enum DefinedAutoBanFlags
		{
			BAN_NONE          = 0x00,
			BAN_BY_MIN_SLOT   = 0x01,
			BAN_BY_MAX_SLOT   = 0x02,
			BAN_BY_SHARE      = 0x04, //-V112
			BAN_BY_LIMIT      = 0x08
		};
		
		DefinedAutoBanFlags hasAutoBan(Client *p_Client = NULL);//[+]FlylinkDC
	private:
		enum SupportSlotsFlag
		{
			FLY_SUPPORT_SLOTS_FIRST = 0x00,
			FLY_SUPPORT_SLOTS = 0x01,
			FLY_NSUPPORT_SLOTS = 0x02
		};
		SupportSlotsFlag m_support_slots;
	public:
#endif // IRAINMAN_ENABLE_AUTO_BAN
	
		struct Hash
		{
			size_t operator()(const UserPtr& x) const
			{
				return ((size_t)(&(*x))) / sizeof(User);
			}
		};
		
//#define ENABLE_DEBUG_LOG_IN_USER_CLASS

		User(const CID& aCID) : m_cid(aCID),
#ifdef PPA_INCLUDE_DEAD_CODE
			m_lastDownloadSpeed(0),
#endif
#ifdef IRAINMAN_ENABLE_AUTO_BAN
			m_support_slots(FLY_SUPPORT_SLOTS_FIRST),
#endif
			m_slots(0),
			m_bytesShared(0),
			m_limit(0),
			m_hub_id(0),
			m_ratio_ptr(nullptr),
			m_is_first_init_ratio(false)
		{
#ifdef _DEBUG
			++g_user_counts;
#ifdef ENABLE_DEBUG_LOG_IN_USER_CLASS
			dcdebug(" [!!!!!!]   [!!!!!!]  User::User(const CID& aCID) this = %p, g_user_counts = %d\n", this, g_user_counts);
#endif
#endif
		}
		
		virtual ~User()
		{
#ifdef _DEBUG
			--g_user_counts;
#ifdef ENABLE_DEBUG_LOG_IN_USER_CLASS
			dcdebug(" [!!!!!!]   [!!!!!!]  User::~User() this = %p, g_user_counts = %d\n", this, g_user_counts);
#endif
#endif
			safe_delete(m_ratio_ptr);
		}
		
#ifdef _DEBUG
		static boost::atomic_int g_user_counts; // [!] IRainman fix: Issue 1037 [BUG] ������ ������ ������ User? https://code.google.com/p/flylinkdc/issues/detail?id=1037
#endif
		
		const CID& getCID() const
		{
			return m_cid;
		}
		//[+]FlylinkDC
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		const string& getLastNick() const
		{
#ifdef _DEBUG
//			static boost::atomic_int g_call_counts;
//			if (++g_call_counts % 1000 == 0)
//				dcdebug("User::getLastNick() called %d\n", int(++g_call_counts));
#endif
			return m_nick;
		}
		void setLastNick(const string& p_nick);
		void setIP(const string& p_ip);
		void storeIP(const string& p_ip);
		uint32_t getHubID() const
		{
			return m_hub_id;
		}
		void setHubID(uint32_t p_hub_id)
		{
			dcassert(p_hub_id);
			m_hub_id = p_hub_id;
		}
		
#else
	public:
		void setIP(const string& ip)
		{
			dcassert(!ip.empty());
			m_ip = ip;
		}
		GETSET(string, m_lastNick, LastNick);
		GETM(string, m_ip, IP);
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO
		tstring getLastNickT() const
		{
#ifdef _DEBUG
			//static boost::atomic_int g_call_counts;
			//if (++g_call_counts % 1000 == 0)
			//  dcdebug("User::getLastNickT() called %d\n", g_call_counts); //-V510
#endif
			return Text::toT(getLastNick());
		}
		//[~]FlylinkDC
		//[+]PPA
		GETSET(int64_t, m_bytesShared, BytesShared); // http://code.google.com/p/flylinkdc/issues/detail?id=1109 ����� ��� �������� � ��������� ������ ����
		GETSET(uint32_t, m_limit, Limit);
		GETSET(uint8_t, m_slots, Slots);
		string m_nick;
		string m_last_ip;
#ifdef PPA_INCLUDE_DEAD_CODE
		GETSET(size_t, m_lastDownloadSpeed, LastDownloadSpeed);
#endif
		//[~]PPA
		
		bool isOnline() const
		{
			return isSet(ONLINE);
		}
		bool isNMDC() const
		{
			return isSet(NMDC);
		}
		// [+] IRainman fix.
		bool isServer() const
		{
			return isSet(SERVER);
		}
		bool isFireball() const
		{
			return isSet(FIREBALL);
		}
		bool isAway() const
		{
			return isSet(AWAY);
		}
		// [~] IRainman fix.
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		void AddRatioUpload(const string& p_ip, uint64_t p_size);
		void AddRatioDownload(const string& p_ip, uint64_t p_size);
		void flushRatio();
		tstring getUDratio();
		tstring getUpload();
		tstring getDownload();
		
		uint64_t getBytesUploadRAW() const
		{
			if(m_ratio_ptr)
			  return m_ratio_ptr->m_upload;
			else
			 return 0;
		}
		uint64_t getBytesDownloadRAW() const
		{
			if(m_ratio_ptr)
			 return m_ratio_ptr->m_download;
			else
			 return 0;
		}
		bool isLastIP() // [+] IRainman fix.
		{
			if(m_ratio_ptr)
			{
					return !m_ratio_ptr->m_last_ip_sql.empty() && m_last_ip.empty();
			}
			else
			{
					return false;
			}
		}
		const string& getIP();
		uint64_t getBytesUpload();
		uint64_t getBytesDownload();
		void initRatio(bool p_is_create);

#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO
	private:
		const CID m_cid; // [!] IRainman fix: this is const value!
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		CFlyUserRatioInfo* m_ratio_ptr;
		uint32_t  m_hub_id;
		bool      m_is_first_init_ratio;
#endif
};

#endif // !defined(USER_H)

/**
 * @file
 * $Id: User.h 551 2010-12-18 12:14:16Z bigmuscle $
 */
/*
 * Copyright (C) 2011-2015 FlylinkDC++ Team http://flylinkdc.com/
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

#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/trim_all.hpp>
#include <boost/algorithm/string.hpp>

#include "flyServer.h"
#include "../client/Socket.h"
#include "../client/ClientManager.h"
#include "../client/ShareManager.h"
#include "../client/SimpleXML.h"
#include "../client/CompatibilityManager.h"
#include "../client/Wildcards.h"
#ifdef PPA_INCLUDE_IPGUARD
#include "../client/IpGuard.h"
#endif
#include "../windows/ChatBot.h"

#include "../jsoncpp/include/json/value.h"
#include "../jsoncpp/include/json/reader.h"
#include "../jsoncpp/include/json/writer.h"
#include "../MediaInfoLib/Source/MediaInfo/MediaInfo_Const.h"
#include "../MediaInfoLib/Source/MediaInfo/MediaInfo.h"
#include "../MediaInfoLib/Source/MediaInfo/MediaInfo_Config.h"

#include "../windows/resource.h"
#include "../client/FavoriteManager.h"
#include "../client/syslog/syslog.h"

#ifdef IRAINMAN_INCLUDE_GDI_OLE
#include "../GdiOle/GDIImage.h"
#endif

#include "ZenLib/ZtringListList.h"


#ifdef FLYLINKDC_USE_GATHER_STATISTICS
#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
  #define PSAPI_VERSION 1
#endif // FLYLINKDC_SUPPORT_WIN_VISTA
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif // FLYLINKDC_USE_GATHER_STATISTICS

string g_debug_fly_server_url;
CFlyServerConfig g_fly_server_config;
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
CFlyServerStatistics g_fly_server_stat;
CServerItem CFlyServerConfig::g_stat_server;
#define FLY_SHUTDOWN_FILE_MARKER_NAME "FlylinkDCShutdownMarker.txt"
#endif // FLYLINKDC_USE_GATHER_STATISTICS
#ifdef STRONG_USE_DHT
std::vector<DHTServer>	  CFlyServerConfig::g_dht_servers;
#endif // STRONG_USE_DHT
DWORD CFlyServerConfig::g_winet_connect_timeout = 2000;
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
static volatile long g_running;
CServerItem CFlyServerConfig::g_test_port_server;
StringSet CFlyServerConfig::g_include_tag; 
StringSet CFlyServerConfig::g_exclude_tag; 
boost::unordered_set<unsigned> CFlyServerConfig::g_exclude_error_log;
boost::unordered_set<unsigned> CFlyServerConfig::g_exclude_error_syslog;
std::vector<CServerItem> CFlyServerConfig::g_mirror_read_only_servers;
std::vector<CServerItem> CFlyServerConfig::g_mirror_test_port_servers;
CServerItem CFlyServerConfig::g_main_server;
uint16_t CFlyServerAdapter::CFlyServerQueryThread::g_minimal_interval_in_ms  = 2000; 

uint16_t CFlyServerConfig::g_winet_min_response_time_for_log = 200;
DWORD CFlyServerConfig::g_winet_receive_timeout = 1000; 
DWORD CFlyServerConfig::g_winet_send_timeout    = 1000; 

std::unordered_map<TTHValue, std::pair<CFlyServerInfo*, CFlyServerCache> > CFlyServerAdapter::g_fly_server_cache;
::CriticalSection CFlyServerAdapter::g_cs_fly_server;
::CriticalSection CFlyServerAdapter::CFlyServerJSON::g_cs_error_report;
string CFlyServerAdapter::CFlyServerJSON::g_last_error_string;
int CFlyServerAdapter::CFlyServerJSON::g_count_dup_error_string = 0;

#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
uint16_t CFlyServerConfig::g_min_interval_dth_connect = 60; // � DHT ���������� �� ���� ��� � 60 ������ (����� ������� ������ ��� ����������)
uint16_t CFlyServerConfig::g_max_ddos_connect_to_me = 10; // �� ����� 10 ��������� �� ���� IP � ������� ������
uint16_t CFlyServerConfig::g_ban_ddos_connect_to_me = 10; // ��������� ����������� � ����� IP � ������� 10 �����

uint16_t CFlyServerConfig::g_interval_flood_command = 1;  // ������� ������ ���������� ���������� �������
uint16_t CFlyServerConfig::g_max_flood_command = 20;       // �� ����� 5 ���������� ������ � �������
uint16_t CFlyServerConfig::g_ban_flood_command = 10;      // ��������� �� 10 ������ ������� ���� ������ � ���

uint16_t CFlyServerConfig::g_max_unique_tth_search  = 10; // �� ��������� � ������� 10 ������ ���������� ������� �� TTH ��� ������ � ����-�� �������� IP:PORT (UDP)
#ifdef USE_SUPPORT_HUB
string CFlyServerConfig::g_support_hub = "dchub://dc.fly-server.ru";
#endif // USE_SUPPORT_HUB
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
string CFlyServerConfig::g_antivirus_db_url;
#endif
string CFlyServerConfig::g_faq_search_does_not_work = "http://www.flylinkdc.ru/2014/01/flylinkdc.html";
StringSet CFlyServerConfig::g_parasitic_files;
StringSet CFlyServerConfig::g_mediainfo_ext;
StringSet CFlyServerConfig::g_virus_ext;
StringSet CFlyServerConfig::g_ignore_flood_command;
StringSet CFlyServerConfig::g_block_share_ext;
StringSet CFlyServerConfig::g_custom_compress_ext;
StringSet CFlyServerConfig::g_block_share_name;
StringList CFlyServerConfig::g_block_share_mask;
extern ::tstring g_full_user_agent;
//======================================================================================================
const CServerItem& CFlyServerConfig::getStatServer()
{
	return g_stat_server;
}
//======================================================================================================
const CServerItem& CFlyServerConfig::getTestPortServer()
{
	return g_test_port_server;
}
//======================================================================================================
const std::vector<CServerItem>& CFlyServerConfig::getMirrorTestPortServerArray()
{
	dcassert(!g_mirror_test_port_servers.empty());
	if (g_mirror_test_port_servers.empty())
	{
		g_mirror_test_port_servers.push_back(g_test_port_server);
	}
    return g_mirror_test_port_servers;
}
//======================================================================================================
const CServerItem& CFlyServerConfig::getRandomMirrorServer(bool p_is_set)
{
	if(p_is_set == false && !g_mirror_read_only_servers.empty())
	{
	 const int l_id = Util::rand(g_mirror_read_only_servers.size());
	 return g_mirror_read_only_servers[l_id];
	}
	else // ������ ������� ������
	{
		return g_main_server;
	}
}
//======================================================================================================
bool CFlyServerConfig::isErrorSysLog(unsigned p_error_code)
{
    return g_exclude_error_syslog.find(p_error_code) == g_exclude_error_syslog.end();
}
//======================================================================================================
bool CFlyServerConfig::isErrorLog (unsigned p_error_code)
{
    return g_exclude_error_log.find(p_error_code) == g_exclude_error_log.end();
}
//======================================================================================================
bool CFlyServerConfig::isSupportTag(const string& p_tag)
{
	const auto l_string_pos = p_tag.find("/String");
	if(l_string_pos != string::npos)
	{
		return false;
	}
	if(g_include_tag.empty())
	{
		return g_exclude_tag.find(p_tag) == g_exclude_tag.end();
	}
	else
	{
		return g_include_tag.find(p_tag) != g_include_tag.end();
	}
}
//======================================================================================================
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
void CFlyServerStatistics::saveShutdownMarkers()
{
	try
	{
	 File l_file (Util::getConfigPath() + FLY_SHUTDOWN_FILE_MARKER_NAME, File::WRITE, File::CREATE | File::TRUNCATE);
	 l_file.write(Util::toString(m_time_mark[TIME_SHUTDOWN_CORE]) + ',' + Util::toString(m_time_mark[TIME_SHUTDOWN_GUI]));
	}
	catch (const FileException&)
	{
	}
}
//======================================================================================================
bool CFlyServerConfig::isSupportFile(const string& p_file_ext, uint64_t p_size) const
{
	dcassert(!m_scan.empty()); // TODO: fix CFlyServerConfig::loadConfig() in debug.
	return p_size > m_min_file_size && m_scan.find(p_file_ext) != m_scan.end(); // [!] IRainman opt.
}
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
//======================================================================================================
bool CFlyServerConfig::isParasitFile(const string& p_file)
{
	return isCheckName(g_parasitic_files, p_file); // [!] IRainman opt.
}
//======================================================================================================
#ifdef STRONG_USE_DHT
const DHTServer& CFlyServerConfig::getRandomDHTServer()
{
	dcassert(!g_dht_servers.empty());
	if(!g_dht_servers.empty())
	{
	 const int l_id = Util::rand(g_dht_servers.size());
	 return g_dht_servers[l_id];
	}
	else
	{
		// fix https://crash-server.com/DumpGroup.aspx?ClientID=ppa&DumpGroupID=113332
		// TODO - ���������� ���������.
		g_dht_servers.push_back(DHTServer("http://ssa.in.ua/dcDHT.php", "")); 
		return g_dht_servers[0];
	}
}
#endif // STRONG_USE_DHT
//======================================================================================================
inline static void checkStrKeyCase(const string& p_str)
{
#ifdef _DEBUG
	string l_str_copy = p_str;
	boost::algorithm::trim(l_str_copy);
	dcassert(l_str_copy == p_str);
#endif
}
//======================================================================================================
inline static void checkStrKey(const string& p_str) // TODO: move to Util.
{
	dcassert(Text::toLower(p_str) == p_str);
	checkStrKeyCase(p_str);
}
//======================================================================================================
void CFlyServerConfig::ConvertInform(string& p_inform) const
{
	// TODO - ������ ������ ������ �� ����������
	if(!m_exclude_tag_inform.empty())
	{
	string l_result_line;
	int  l_start_line = 0;
	auto l_end_line = string::npos;
	do
	{
	l_end_line = p_inform.find("\r\n",l_start_line);
	if (l_end_line != string::npos)
	 {
		string l_cur_line = p_inform.substr(l_start_line,l_end_line - l_start_line);
		// ��������� ������� ����� �����
		bool l_ignore_tag = false;
		for(auto i = m_exclude_tag_inform.cbegin(); i != m_exclude_tag_inform.cend(); ++i)
		{
			const auto l_tag_begin = l_cur_line.find(*i);
			const auto l_end_index  = i->size() + 1;
			if(l_tag_begin != string::npos 
				&& l_tag_begin == 0  // ��� � ������ ������?
				&& l_cur_line.size() > l_end_index // ����� ���� ���� �����?
				&& (l_cur_line[l_end_index] == ':' || l_cur_line[l_end_index] == ' ') // ����� ���� ������ ��� ':'
				)
			{ 
				l_ignore_tag = true;
				break;
			}
		}
		if(l_ignore_tag == false)
		{
			  boost::algorithm::trim_all(l_cur_line);
			  l_result_line += l_cur_line + "\r\n";
		}
		l_start_line = l_end_line + 2;
	 }
	}
	while (l_end_line != string::npos);
    p_inform = l_result_line;	
  }
}
//======================================================================================================
void CFlyServerConfig::loadConfig()
{
	const auto l_cur_tick = GET_TICK();
	if(m_time_load_config == 0 || (l_cur_tick - m_time_load_config) > m_time_reload_config ) 
	{
		m_time_load_config = l_cur_tick + 1;
		CFlyLog l_fly_server_log("[fly-server]");
		LPCSTR l_res_data;
		std::string l_data;
#ifdef _DEBUG
    //#define USE_FLYSERVER_LOCAL_FILE
#endif
#ifdef USE_FLYSERVER_LOCAL_FILE
		const string l_url_config_file = "file://C:/vc10/etc/flylinkdc-config-r5xx.xml"; 
		g_debug_fly_server_url = "localhost";
                // g_debug_fly_server_url = "flylinkdc.no-ip.org";
#else
		const string l_url_config_file = "http://etc.fly-server.ru/etc/flylinkdc-config-r5xx.xml"; // TODO etc.fly-server.ru
#endif
		l_fly_server_log.step("Download:" + l_url_config_file);
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		if (Util::getDataFromInet(l_url_config_file, l_data, 0) == 0)
		{
			l_fly_server_log.step("Error download! Config will be loaded from internal resources");
#endif //FLYLINKDC_USE_MEDIAINFO_SERVER
			if (const auto l_size_res = Util::GetTextResource(IDR_FLY_SERVER_CONFIG, l_res_data))
      {
				l_data = string(l_res_data,l_size_res);
      }
      else
      {
          l_fly_server_log.step("Error load resource Util::GetTextResource(IDR_FLY_SERVER_CONFIG");
      }
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		}
#endif //FLYLINKDC_USE_MEDIAINFO_SERVER
		try
		{
			SimpleXML l_xml;
			l_xml.fromXML(l_data);
			if (l_xml.findChild("Bootstraps"))
			{
#ifdef STRONG_USE_DHT
				if(g_dht_servers.empty()) // DHT �������� ���� ���
				{
					l_xml.stepIn();
					while (l_xml.findChild("server"))
					{
						const string& l_server = l_xml.getChildAttrib("url");
						if (!l_server.empty())
						{
							const string& l_agent = l_xml.getChildAttrib("agent");
							g_dht_servers.push_back(DHTServer(l_server, l_agent));
						}
					}
					l_xml.stepOut();
				}
#endif // STRONG_USE_DHT
				l_xml.stepIn();
				if (l_xml.findChild("fly-server"))
				{
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
					g_main_server.setIp(l_xml.getChildAttrib("ip"));
					g_main_server.setPort(Util::toInt(l_xml.getChildAttrib("port")));
					dcassert(g_main_server.getPort());
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
					// ��������������� �������������� ������ ����������
					g_stat_server.setIp(l_xml.getChildAttrib("stat_server_ip")); 
					if(!g_stat_server.getIp().empty())
					{
						g_stat_server.setPort(Util::toInt(l_xml.getChildAttrib("stat_server_port")));
						dcassert(g_stat_server.getPort());
						if(g_stat_server.getPort() == 0)
						{
							g_stat_server.setPort(g_main_server.getPort());
						}
					}
					else
					{
						g_stat_server = g_main_server;
					}
#endif
					// ��������������� �������������� ������ ������������ ������
					g_test_port_server.setIp(l_xml.getChildAttrib("test_server_ip")); 
					if(!g_test_port_server.getIp().empty())
					{
						g_test_port_server.setPort(Util::toInt(l_xml.getChildAttrib("test_server_port")));
						dcassert(g_test_port_server.getPort());
						if(g_test_port_server.getPort() == 0)
						{
							g_test_port_server.setPort(g_main_server.getPort());
						}
					}
					else
					{
						g_test_port_server = g_main_server;
					}	
					auto initUINT16 = [&](const string& p_name, uint16_t& p_value, uint16_t p_min) -> void
					{
						const string l_value = l_xml.getChildAttrib(p_name);
						if(!l_value.empty())
						{
							p_value = Util::toInt(l_value);
							if(p_value < p_min)
							  p_value = p_min;
						}
					};
					auto initDWORD = [&](const string& p_name, DWORD& p_value) -> void
					{
						const string l_value = l_xml.getChildAttrib(p_name);
						if(!l_value.empty())
						{
							p_value = Util::toInt(l_value);
						}
					};
					auto initString = [&](const string& p_name, string& p_value) -> void
					{
						const string l_value = l_xml.getChildAttrib(p_name);
						if(!l_value.empty())
					{
							p_value = l_value;
					}					
					};
					initUINT16("max_unique_tth_search",g_max_unique_tth_search,3);
					initUINT16("max_ddos_connect_to_me",g_max_ddos_connect_to_me,3);
					initUINT16("ban_ddos_connect_to_me",g_ban_ddos_connect_to_me,3);
          
					initUINT16("interval_flood_command",g_interval_flood_command,1);
					initUINT16("max_flood_command",g_max_flood_command,20);
					initUINT16("ban_flood_command",g_ban_flood_command,10);

					initUINT16("min_interval_dth_connect",g_min_interval_dth_connect,60); // ���� ��� � XML
					initDWORD("winet_connect_timeout",g_winet_connect_timeout);
					initDWORD("winet_receive_timeout",g_winet_receive_timeout);
					initDWORD("winet_send_timeout",g_winet_send_timeout);
					initUINT16("winet_min_response_time_for_log",g_winet_min_response_time_for_log,50);

					m_min_file_size = Util::toInt64(l_xml.getChildAttrib("min_file_size")); // � ������� min_size - �������������
					dcassert(m_min_file_size);
					//m_max_size_value = Util::toInt(l_xml.getChildAttrib("max_size_value")); // �� ������������ �� ���� � ������� - ������ �����
					//dcassert(m_max_size_value);
					m_zlib_compress_level = Util::toInt(l_xml.getChildAttrib("zlib_compress_level"));
					dcassert(m_zlib_compress_level >= Z_NO_COMPRESSION && m_zlib_compress_level <= Z_BEST_COMPRESSION);
					if(m_zlib_compress_level <= Z_NO_COMPRESSION || m_zlib_compress_level > Z_BEST_COMPRESSION)
					{
						m_zlib_compress_level = Z_BEST_COMPRESSION;
					}
					m_send_full_mediainfo = Util::toInt(l_xml.getChildAttrib("send_full_mediainfo")) == 1;
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
					initString("antivirus_db",g_antivirus_db_url);
#endif
#ifdef USE_SUPPORT_HUB
					initString("support_hub",g_support_hub);
#endif // USE_SUPPORT_HUB
					initString("faq_search",g_faq_search_does_not_work);
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER_COLLECT_LOST_LOCATION
					m_collect_lost_location = Util::toInt(l_xml.getChildAttrib("collect_lost_location")) == 1;
#endif
					m_type = l_xml.getChildAttrib("type") == "http" ? TYPE_FLYSERVER_HTTP : TYPE_FLYSERVER_TCP ;
					CFlyServerAdapter::CFlyServerQueryThread::setMinimalIntervalInMilliSecond(Util::toInt(l_xml.getChildAttrib("minimal_interval")));
					l_xml.getChildAttribSplit("scan", m_scan, [this](const string& n)
					{
						checkStrKey(n);
						m_scan.insert(n);
			        });

					l_xml.getChildAttribSplit("exclude_tag", g_exclude_tag, [this](const string& n)
					{
						g_exclude_tag.insert(n);
					});
					l_xml.getChildAttribSplit("include_tag", g_include_tag, [this](const string& n)
					{
						g_include_tag.insert(n);
					});
					l_xml.getChildAttribSplit("exclude_error_log", g_exclude_error_log, [this](const string& n)
					{
              g_exclude_error_log.insert(Util::toInt(n));
					});
					l_xml.getChildAttribSplit("exclude_error_syslog", g_exclude_error_syslog, [this](const string& n)
					{
              g_exclude_error_syslog.insert(Util::toInt(n));
					});
					// �������� RO-�������
					l_xml.getChildAttribSplit("mirror_read_only_server", g_mirror_read_only_servers, [this](const string& n)
					{
						const auto l_port_pos = n.find(':');
						if(l_port_pos != string::npos)
							g_mirror_read_only_servers.push_back(CServerItem(n.substr(0, l_port_pos), atoi(n.c_str() + l_port_pos + 1)));
					});
					// �������� ������� �������� ��� ������
					l_xml.getChildAttribSplit("mirror_test_port_server", g_mirror_test_port_servers, [this](const string& n)
					{
						const auto l_port_pos = n.find(':');
						if(l_port_pos != string::npos)
							g_mirror_test_port_servers.push_back(CServerItem(n.substr(0, l_port_pos), atoi(n.c_str() + l_port_pos + 1)));
					});
          

#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
					l_xml.getChildAttribSplit("exclude_tag_inform", m_exclude_tag_inform, [this](const string& n)
					{
						m_exclude_tag_inform.push_back(n);
					});

					l_xml.getChildAttribSplit("parasitic_file", g_parasitic_files, [this](const string& n)
					{
						g_parasitic_files.insert(n);
					});

					l_xml.getChildAttribSplit("mediainfo_ext", g_mediainfo_ext, [this](const string& n)
					{
						checkStrKey(n);
						g_mediainfo_ext.insert(n);
					});
					l_xml.getChildAttribSplit("virus_ext", g_virus_ext, [this](const string& n)
					{
						checkStrKey(n);
						g_virus_ext.insert(n);
					});
					l_xml.getChildAttribSplit("ignore_flood_command", g_ignore_flood_command, [this](const string& n)
					{
						checkStrKeyCase(n);
						g_ignore_flood_command.insert(n);
					});
					l_xml.getChildAttribSplit("block_share_ext", g_block_share_ext, [this](const string& n)
					{
						checkStrKey(n);
						g_block_share_ext.insert(n);
					});
					g_block_share_ext.insert(g_dc_temp_extension);
					l_xml.getChildAttribSplit("custom_compress_ext", g_custom_compress_ext, [this](const string& n)
					{
						checkStrKey(n);
						g_custom_compress_ext.insert(n);
					});
					l_xml.getChildAttribSplit("block_share_name", g_block_share_name, [this](const string& n)
					{
						checkStrKey(n);
						g_block_share_name.insert(n);
					});
					l_xml.getChildAttribSplit("block_share_mask", g_block_share_mask, [this](const string& n)
					{
						checkStrKey(n);
						g_block_share_mask.push_back(n);
					});
				}
				l_xml.stepOut();
			}
			l_fly_server_log.step("Download and parse - Ok!");
			m_time_reload_config = TIME_TO_RELOAD_CONFIG_IF_SUCCESFUL;
			m_time_load_config = l_cur_tick;
		}
		catch (const Exception& e)
		{
			dcassert(0);
			dcdebug("CFlyServerConfig::loadConfig parseXML ::Problem: %s\n", e.what());
			l_fly_server_log.step("parseXML Problem:" + e.getError());
		}
    dcassert(!g_ignore_flood_command.empty());
  	if(g_ignore_flood_command.empty())
		{
						g_ignore_flood_command.insert("Search");
						g_ignore_flood_command.insert("UserCommand");
						g_ignore_flood_command.insert("Quit"); 
						g_ignore_flood_command.insert("MyINFO"); 
						g_ignore_flood_command.insert("ConnectToMe"); 
						g_ignore_flood_command.insert("UserIP");
						g_ignore_flood_command.insert("RevConnectToMe");
		}
	}
}
//======================================================================================================
bool CFlyServerConfig::SyncAntivirusDB()
{
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
#ifndef USE_FLYSERVER_LOCAL_FILE
  dcassert(!g_antivirus_db_url.empty());
  if(BOOLSETTING(AUTOUPDATE_ANTIVIRUS_DB))
  {
  if(!g_antivirus_db_url.empty())
  {
  bool l_is_change_version = false;
  uint64_t l_time_stamp = CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_TimeStampAntivirusDB);
  const auto l_start_sync = GET_TIME();
  CFlyLog l_log("[Sync Antivirus DB]");
  string l_buf;
  std::vector<byte> l_binary_data;
  CFlyHTTPDownloader l_http_downloader;
#ifdef _DEBUG
 //  CFlylinkDBManager::getInstance()->set_registry_variable_int64(e_DeleteCounterAntivirusDB,0);
#endif
  __int64 l_cur_merge_counter = 0;
  for(int i=0;i<2;++i)
  {
    l_http_downloader.m_get_http_header_item.clear();
    l_http_downloader.m_get_http_header_item.push_back("Avdb-Delete-Count");
    l_http_downloader.m_get_http_header_item.push_back("Avdb-Version-Count");
    l_cur_merge_counter = CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_MergeCounterAntivirusDB);
    const string l_url = g_antivirus_db_url + 
                   "/avdb.php?do=load"
                   "&time=" + Util::toString(l_time_stamp) + 
                   "&vers=" + Util::toString(l_cur_merge_counter) +
                   "&cotime=0"
                   "&nosort=1"
                   "&copath=1";
    l_binary_data.clear();
	auto l_result_size = l_http_downloader.getBinaryDataFromInet(l_url, l_binary_data, g_winet_connect_timeout/2); 
	if(l_result_size == 0)
     { 
		 l_log.step("Antivirus DB error download. " + l_url);
		 return false;
     }
    if(!l_http_downloader.m_get_http_header_item[0].empty())
    {
        const int  l_new_delete_counter = Util::toInt(l_http_downloader.m_get_http_header_item[0]);
		dcassert(l_new_delete_counter);
	    const auto l_cur_delete_counter = CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_DeleteCounterAntivirusDB);
        if(l_cur_delete_counter != l_new_delete_counter)
        {
            if(l_time_stamp == 0)
            {
              CFlylinkDBManager::getInstance()->purge_antivirus_db(l_new_delete_counter,l_start_sync);
              break;
            }
            else
            {
            l_time_stamp = 0;
              CFlylinkDBManager::getInstance()->purge_antivirus_db(l_new_delete_counter,l_time_stamp);
            }
            l_log.step("Reload antivirus DB Avdb-Delete-Count = " + Util::toString(l_new_delete_counter));
            continue; 
        }
    }
    if(l_result_size > 1) 
    {
      l_buf = string((char*)l_binary_data.data(), l_result_size);
    }
   break;
  }
   if (!l_buf.empty())
   {
    const int l_new_merge_counter = Util::toInt(l_http_downloader.m_get_http_header_item[1]);
	dcassert(l_new_merge_counter);
    CFlylinkDBManager::getInstance()->set_registry_variable_int64(e_MergeCounterAntivirusDB,l_new_merge_counter);
	if(l_new_merge_counter != l_cur_merge_counter)
	{
	 l_is_change_version = true;
     l_cur_merge_counter = l_new_merge_counter;
	}
    const auto l_count = CFlylinkDBManager::getInstance()->sync_antivirus_db(l_buf,l_start_sync);
    if(l_count)
	{
	    l_log.step("Add new records: " + Util::toString(l_count));
	}
   }
   if(l_cur_merge_counter)
   {
	 // TODO ���������� N-������� � ���� - ����������� �� � ���
     l_log.step("Antivirus DB version: " + Util::toString(l_cur_merge_counter));
   }
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
   if(l_is_change_version)
   {
        ClientManager::resetAntivirusInfo(); 
   }
#endif
  }
  }
#endif
#endif
  return true;
}
//======================================================================================================
bool CFlyServerConfig::isIgnoreFloodCommand(const string& p_command)
{
	return isCheckName(g_ignore_flood_command, p_command);
}
bool CFlyServerConfig::isVirusExt(const string& p_ext)
{
	return isCheckName(g_virus_ext, p_ext);
}
bool CFlyServerConfig::isMediainfoExt(const string& p_ext)
{
	return isCheckName(g_mediainfo_ext, p_ext);
}
bool CFlyServerConfig::isCompressExt(const string& p_ext)
{
	return isCheckName(g_custom_compress_ext, p_ext);
}
bool CFlyServerConfig::isBlockShare(const string& p_name)
{
	dcassert(Text::toLower(p_name) == p_name);
	
	if(isCheckName(g_block_share_ext, Util::getFileExtWithoutDot(p_name))) // �������� �� �����������
	{
	   return true;
	}
	else if(isCheckName(g_block_share_name, p_name)) // �������� �� ������ �� ������ �����������
	{
	   return true;
	}
	else if (Wildcard::patternMatchLowerCase(p_name, g_block_share_mask)) // ����� ��� ����� ������
	{ 
		return true;
	}
	return false; // ���� �������
}

string CFlyServerConfig::DBDelete()
{
	string l_where;
	string l_or;
	for (auto i = g_mediainfo_ext.cbegin(); i !=  g_mediainfo_ext.cend(); ++i)
	{
		l_where = l_where + l_or + string("lower(name) like '%.") + *i + string("'");
		if (l_or.empty())
			l_or = " or ";
	}
	return l_where;
}
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
//======================================================================================================
//string sendTTH(const CFlyServerKey& p_fileInfo, bool p_send_mediainfo)
//{
//	CFlyServerKeyArray l_array;
//	l_array.push_back(p_fileInfo);
//	return sendTTH(l_array,p_send_mediainfo);
//}
//======================================================================================================
string CFlyServerAdapter::CFlyServerJSON::g_fly_server_id;
CFlyTTHKeyArray CFlyServerAdapter::CFlyServerJSON:: g_download_counter;
//===================================================================================================================================
void CFlyServerAdapter::post_message_for_update_mediainfo()
{
		if (!m_GetFlyServerArray.empty())
		{
			const string l_json_result = CFlyServerJSON::connect(m_GetFlyServerArray, false); 
			{
				Lock l(g_cs_fly_server);
				m_GetFlyServerArray.clear();
			}
			if(!l_json_result.empty())
			{
			Json::Value* l_root = new Json::Value;
			Json::Reader l_reader(Json::Features::strictMode());
			const bool l_parsingSuccessful = l_reader.parse(l_json_result, *l_root);
			if (!l_parsingSuccessful && !l_json_result.empty())
			{
				{
					Lock l(g_cs_fly_server);
					m_tth_media_file_map.clear();  // ���� �������� ������ �������� ������� �� ������, ������ �� ����.
				}
				delete l_root;
				LogManager::getInstance()->message("Failed to parse json configuration: l_json_result = " + l_json_result);
			}
			else
			{
				PostMessage(m_hMediaWnd, WM_SPEAKER_MERGE_FLY_SERVER, WPARAM(l_root),LPARAM(NULL));
			}
			}
			else
			{
				Lock l(g_cs_fly_server);
				m_tth_media_file_map.clear(); // ���� �������� ������ �������� ������� �� ������, ������ �� ����.
			}
		}
		push_mediainfo_to_fly_server(); // ������� �� ����-������ ���������, ��� ����� � ���� (��� �� ��� ���)
}
//===================================================================================================================================
void CFlyServerAdapter::push_mediainfo_to_fly_server()
{
	CFlyServerKeyArray l_copy_map;
	{
		Lock l(g_cs_fly_server);
		l_copy_map.swap(m_SetFlyServerArray);
	}
	if (!l_copy_map.empty())
	{
		CFlyServerJSON::connect(l_copy_map, true);
	}
}
//======================================================================================================
void CFlyServerAdapter::prepare_mediainfo_to_fly_serverL()
{
	// ������� ���������� ��� ������� �� ������.
	// ������ - ���� � ��� � ����, �� ��� �� fly-server
	for (auto i = m_tth_media_file_map.begin(); i != m_tth_media_file_map.end(); ++i)
	{
		CFlyMediaInfo l_media_info;
		if (CFlylinkDBManager::getInstance()->load_media_info(i->first, l_media_info, false))
		{
			bool l_is_send_info = l_media_info.isMedia() && g_fly_server_config.isFullMediainfo() == false; // ���� ��������� � ������ �� ���� ������ ��������?
			if (g_fly_server_config.isFullMediainfo()) // ���� ������ ���� �� ��� ������ ������ �������� - �������� ������� ���������� ������������
				l_is_send_info = l_media_info.isMediaAttrExists();
			if (l_is_send_info)
			{
				CFlyServerKey l_info(i->first, i->second);
				l_info.m_media = l_media_info; // �������� ��������������� � ��������� ����
				m_SetFlyServerArray.push_back(l_info);
			}
		}
	}
	m_tth_media_file_map.clear();
}
//======================================================================================================
bool CFlyServerAdapter::CFlyServerJSON::login()
{
  bool l_is_error = false;
	if(g_fly_server_id.empty())
	{
		CFlyLog l_log("[fly-login]");
		Json::Value  l_root;   
		Json::Value& l_info = l_root["login"];
		l_info["CID"] = ClientManager::getMyCID().toBase32();
		l_info["PID"] = ClientManager::getMyPID().toBase32();
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER_COLLECT_LOST_LOCATION
		std::vector<std::string> l_lost_ip_array;
		CFlylinkDBManager::getInstance()->get_lost_location(l_lost_ip_array);
		if(!l_lost_ip_array.empty())
		{
			Json::Value& l_lost_ip = l_info["customlocation_lost_ip"];
			int l_count_lost = 0;
			for (auto i = l_lost_ip_array.cbegin(); i!= l_lost_ip_array.cend(); ++i)
			{
				l_lost_ip[l_count_lost++] = *i;
			}
		}
#endif
		const std::string l_post_query = l_root.toStyledString();
		bool l_is_send = false;
		string l_result_query = postQuery(true,false,false,false,false,"fly-login",l_post_query,l_is_send,l_is_error);
		Json::Value l_result_root;
		Json::Reader l_reader(Json::Features::strictMode());
		const bool l_parsingSuccessful = l_reader.parse(l_result_query, l_result_root);
		if (!l_parsingSuccessful && !l_result_query.empty())
		{
			l_log.step("Failed to parse json configuration: l_result_query = " + l_result_query);
		}
		else
		{
			g_fly_server_id = l_result_root["ID"].asString();
			if(!g_fly_server_id.empty())
				l_log.step("Register OK!");
//			else
//				g_fly_server_id
		}
	}
  return l_is_error;
}
static void getDiskAndMemoryStat(Json::Value& p_info)
{
		if(ClientManager::isValidInstance())
		{
			  p_info["CID"] = ClientManager::getMyCID().toBase32(); 
			  p_info["PID"] = ClientManager::getMyPID().toBase32();
		}
		p_info["Client"] = Text::fromT(g_full_user_agent);
		p_info["OS"] = CompatibilityManager::getFormatedOsVersion();
		p_info["CPUCount"] = CompatibilityManager::getProcessorsCount();
		{
		Json::Value& l_disk_info = p_info["Disk"];
				auto getFileSize = [](const ::tstring& p_file_name) -> int64_t
				{
					int64_t l_size = 0;
					int64_t l_outFileTime = 0;
					File::isExist(p_file_name, l_size, l_outFileTime);
					return l_size;
				};
				const auto l_path = Text::toT(Util::getConfigPath());
				l_disk_info["DBMain"] = getFileSize(l_path + _T("\\FlylinkDC.sqlite"));
				l_disk_info["DBDHT"] = getFileSize(l_path + _T("\\FlylinkDC_dht.sqlite"));
				l_disk_info["DBMediainfo"] = getFileSize(l_path + _T("\\FlylinkDC_mediainfo.sqlite"));
				l_disk_info["DBLog"] = getFileSize(l_path + _T("\\FlylinkDC_log.sqlite"));
				l_disk_info["DBStat"] = getFileSize(l_path + _T("\\FlylinkDC_stat.sqlite"));
				l_disk_info["DBUser"] = getFileSize(l_path + _T("\\FlylinkDC_user.sqlite")); 
				l_disk_info["DBAntivirus"] = getFileSize(l_path + _T("\\FlylinkDC_antivirus.sqlite")); 
                l_disk_info["DBTransfer"] = getFileSize(l_path + _T("\\FlylinkDC_transfers.sqlite")); 
				// TODO - ������� ������ ������ �������				

				DWORD l_cluster, l_sector_size, l_freeclustor;
				int64_t l_space;
				if(l_path.size() >= 3)
				{
				if (GetDiskFreeSpace(l_path.substr(0,3).c_str(), &l_cluster, &l_sector_size, &l_freeclustor, NULL))
				{
				 l_space  = l_cluster * l_sector_size;
				 l_space *= l_freeclustor;
				 l_disk_info["DBFreeSpace"] = int64_t(l_space/1024/1024);
				}
				}
		}
		PROCESS_MEMORY_COUNTERS l_pmc = {0};
		const auto l_mem_ok = GetProcessMemoryInfo(GetCurrentProcess(), &l_pmc, sizeof(l_pmc));
		dcassert(l_mem_ok);
		if(l_mem_ok) // ��� Wine ����� �� ��������
		{
		 Json::Value& l_mem_info = p_info["Memory"];
		l_mem_info["WorkingSetSize"]     = int64_t(l_pmc.WorkingSetSize);
		l_mem_info["PeakWorkingSetSize"] = int64_t(l_pmc.PeakWorkingSetSize);
		l_mem_info["TotalPhys"] = CompatibilityManager::getTotalPhysMemory();
		}
		Json::Value& l_handle_info = p_info["Handle"];
		{
		DWORD l_handle_count = 0;
		const auto l_hc_ok = GetProcessHandleCount(GetCurrentProcess(), &l_handle_count);
		dcassert(l_hc_ok);
		l_handle_info["Handle"] = int(l_handle_count); // TODO ������� jsoncpp �������� DWORD
		auto getResourceCounter = [](int p_type_object) -> unsigned
		 {
			const unsigned l_res_count = GetGuiResources(GetCurrentProcess(), p_type_object);
			return l_res_count;
        };

		l_handle_info["GDI"]     = getResourceCounter(GR_GDIOBJECTS);
		l_handle_info["UserObj"] = getResourceCounter(GR_USEROBJECTS);

#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
# define GR_GDIOBJECTS_PEAK  2       /* Peak count of GDI objects */
# define GR_USEROBJECTS_PEAK 4       /* Peak count of USER objects */
#endif 
		 // http://msdn.microsoft.com/en-us/library/windows/desktop/ms683192%28v=vs.85%29.aspx
		 // This value is not supported until Windows 7 and Windows Server 2008 R2.
		l_handle_info["GDIPeak"]      = getResourceCounter(GR_GDIOBJECTS_PEAK);
		l_handle_info["UserObjPeak"]  = getResourceCounter(GR_USEROBJECTS_PEAK);
		}
		// TODO - ��������� ����� �� ����� ��������� � ���, ��� ����� ����-����
		{
			//Json::Value& l_disk_info = l_info["Disk"];
			//l_disk_info["SysFree"] = 
			//l_disk_info["sqliteFree"] = 
		}
		{
		Json::Value& l_screen_info = p_info["Screen"];
		static RECT g_desktop;
		if(g_desktop.right == 0)
		{
	          GetWindowRect(GetDesktopWindow(), &g_desktop);
		}
		l_screen_info["X"] = g_desktop.right;
		l_screen_info["Y"] = g_desktop.bottom;
		// TODO - ���������� ��� NAT
		//	http://ilya-314.livejournal.com/109479.html
		//  http://system-administrators.info/?p=1468
		//  https://github.com/limlabs/stunclient
	}
}
//======================================================================================================
string CFlyServerAdapter::CFlyServerJSON::postQueryTestPort(CFlyLog& p_log,const string& p_body, bool& p_is_send, bool& p_is_error)
{
    string l_result;
    const auto& l_server_array = CFlyServerConfig::getMirrorTestPortServerArray();
    for(auto i=l_server_array.cbegin(); i!=l_server_array.cend() ;++i)
    {
      const auto& l_test_server = *i;
      l_result = postQuery(false,false,true,true,true,"fly-test-port",p_body,p_is_send,p_is_error,1000, &l_test_server);
      if(p_is_error == false && !l_result.empty())
      {
          break;
      }
	  p_log.step("Use next server: " + l_test_server.getServerAndPort());
    }
    return l_result;
}
//======================================================================================================
bool CFlyServerAdapter::CFlyServerJSON::pushTestPort(const string& p_magic,
		const std::vector<unsigned short>& p_udp_port,
		const std::vector<unsigned short>& p_tcp_port,
		string& p_external_ip,
		int p_timer_value)
{
		CFlyLog l_log("[fly-test-port]");
		Json::Value  l_info;   
		l_info["CID"] = p_magic;
		if(p_timer_value)
		{
			l_info["Interval"] = p_timer_value;
		}
		auto initPort = [&](const std::vector<unsigned short>& p_port, const char* p_key) -> void
		{
			if(!p_port.empty())
			{
				auto& l_ports = l_info[p_key];
				for(int i = 0;i < int(p_port.size()); ++i)
		{
					l_ports[i]["port"] = p_port[i];
			}
		}
		};
		initPort(p_udp_port,"udp");
		initPort(p_tcp_port,"tcp");
		if(SETTING(BIND_ADDRESS) != "0.0.0.0")
		{
		  l_info["ip"] = SETTING(BIND_ADDRESS);
		}
		const std::string l_post_query = l_info.toStyledString();
		bool l_is_send = false;
		bool l_is_error = false;
		p_external_ip.clear();
	  const auto l_result = postQueryTestPort(l_log, l_post_query,l_is_send,l_is_error);
    dcassert(!l_result.empty());
		// TODO - ��������� ������� �������� � ���������� ��� � ���������� ��� � ���� �����?
		if(!l_is_send)
		{
					l_log.step("Error POST query");
		}
		else
    if(!l_result.empty())
		{
			Json::Value l_root;
			Json::Reader reader(Json::Features::strictMode()); 
			const bool parsingSuccessful = reader.parse(l_result, l_root);
			if (!parsingSuccessful)
			{
				l_log.step("Error parse JSON: " + l_result);
        dcassert(0);
			}
		else
		{
        dcassert(!l_root.isNull());
        dcassert(l_root.isMember("ip"));
					p_external_ip = l_root["ip"].asString();
		}
		}
		return l_is_send;
}
//======================================================================================================
void CFlyServerAdapter::CFlyServerJSON::pushSyslogError(const string& p_error)
{
    string l_cid;
	string l_pid;	
	if(ClientManager::isValidInstance())
	{
		l_cid = ClientManager::getMyCID().toBase32();
		l_pid = ClientManager::getMyPID().toBase32();
	}
   else
   {
     l_cid = "[CID==null]";
   }
	syslog(LOG_USER | LOG_INFO, "%s %s %s [%s]", l_cid.c_str(), l_pid.c_str(), p_error.c_str(), Text::fromT(g_full_user_agent).c_str());
}
//======================================================================================================
bool CFlyServerAdapter::CFlyServerJSON::pushError(unsigned p_error_code, string p_error)
{
	bool l_is_send = false;
  bool l_is_error = false;
    p_error = "[BUG][" + Util::toString(p_error_code) + "] " + p_error;
    if(CFlyServerConfig::isErrorSysLog(p_error_code))
    {
  pushSyslogError(p_error);
    }
    Lock l(g_cs_error_report);
    if(CFlyServerConfig::isErrorLog(p_error_code))
    {
    CFlyLog l_log("[fly-error]");
    l_log.step(p_error);
    if(p_error != g_last_error_string)
    {
		Json::Value  l_info;   
        if(g_count_dup_error_string == 0)
        {
		l_info["error"] = p_error;
        }
        else
        {
          l_info["error"] = p_error + "[DUP COUNT=" + Util::toString(g_count_dup_error_string) + " [" + g_last_error_string + "]";
        }
		l_info["ID"]  = g_fly_server_id;
		l_info["Threads"]  =  Thread::getThreadsCount();
		l_info["Current"]  = Util::formatDigitalClock(time(nullptr));
		getDiskAndMemoryStat(l_info);
		const std::string l_post_query = l_info.toStyledString();
	    postQuery(true,true,false,false,false,"fly-error-sql",l_post_query,l_is_send,l_is_error,2000);
		if(!l_is_send)
		{
			 // TODO �������� �� ������� - ������ ������ � �����
		}
        g_count_dup_error_string = 0;
        g_last_error_string = p_error;
    }
    else
    {
        g_count_dup_error_string++;
        l_is_send = true;
    }
    }
    else
    {
        l_is_send = true;
    }
		return l_is_send;
}
//======================================================================================================
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
bool CFlyServerAdapter::CFlyServerJSON::pushStatistic(const bool p_is_sync_run)
{
	Thread::ConditionLockerWithSpin l(g_running);
    bool l_is_flush_error = login();
	CFlyLog l_log("[fly-stat]");
	Json::Value  l_info;   
	if(p_is_sync_run == false && l_is_flush_error == false) // ��� �������� �� ������ ����� + ���� ���� ������ ������ - ���� �������
		{
		// ������� 10 ������� ���������� ���������� ���� ����������
		 CFlylinkDBManager::getInstance()->flush_lost_json_statistic(l_is_flush_error);
		}
		else
		{
		l_info["IsShutdown"] = "1"; // �������� ������ �������� ����
		}
		//dcassert(!g_fly_server_id.empty());
		
		getDiskAndMemoryStat(l_info);

		l_info["ID"]  = g_fly_server_id;
		const string l_VID_Array = Util::getRegistryCommaSubkey(_T("VID"));
		if(!l_VID_Array.empty())
		{
			l_info["VID"] = l_VID_Array;
		}
#ifndef USE_STRONGDC_SQLITE
		if(ChatBot::isLoaded())
		{
			l_info["is_chat_bot"] = 1;
		}
		if(SETTING(ENABLE_AUTO_BAN))
		{
			l_info["is_autoban"] = 1;
		}
#endif // USE_STRONGDC_SQLITE
		extern bool g_DisableSQLJournal;
		if (g_DisableSQLJournal || BOOLSETTING(SQLITE_USE_JOURNAL_MEMORY))
		{
			l_info["is_journal_memory"] = 1;
		}
		extern bool g_UseWALJournal;
		if (g_UseWALJournal)
		{
			l_info["is_journal_wal"] = 1;
		}
		extern bool g_UseSynchronousOff;
		if (g_UseSynchronousOff)
		{
			l_info["is_synchronous_off"] = 1;
		}
		//
		const auto l_ISP_URL = SETTING(ISP_RESOURCE_ROOT_URL);
		if(!l_ISP_URL.empty())
		{
			l_info["ISP_URL"] = l_ISP_URL;
		}
    if(!SETTING(FLY_LOCATOR_COUNTRY).empty())
    {
        Json::Value& l_locator =l_info["Locator"];
        l_locator["Country"] = SETTING(FLY_LOCATOR_COUNTRY);
        l_locator["City"] = SETTING(FLY_LOCATOR_CITY);
        l_locator["ISP"] = SETTING(FLY_LOCATOR_ISP);
    }
		// ������������� ���������
		{
		    Json::Value& l_stat_info = l_info["Stat"];
			l_stat_info["Files"] = Util::toString(ShareManager::getInstance()->getSharedFiles());
			l_stat_info["Folders"] = Util::toString(CFlylinkDBManager::getInstance()->get_count_folders());
			l_stat_info["Size"]  = ShareManager::getInstance()->getShareSizeString();
			// TODO - ��� ��������� ����� ��������� �� ������� Clients
			l_stat_info["Users"] = Util::toString(ClientManager::getTotalUsers());
			l_stat_info["Hubs"]  = Util::toString(Client::getTotalCounts());
			{
				const auto l_stat = ClientManager::getClientStat();
				if(!l_stat.empty())
				{
				 int j = 0;
				 for(auto i = l_stat.cbegin(); i != l_stat.cend();++i )
				 {
					 auto& l_item = l_stat_info["Clients"][j++];
					 l_item["url"]   = i->first;
					 if(!i->second.empty())
					 {
					  l_item["Count"] = i->second.m_count_user;
					  l_item["Share"] = i->second.m_share_size;
					  if(i->second.m_message_count)
					  {
							l_item["Messages"] = i->second.m_message_count;
					  }
					 }
				 }
				}
			}
			// TODO l_stat_info["MaxUsers"] = 
			l_stat_info["DBQueueSources"] = CFlylinkDBManager::getCountQueueSources();        
			l_stat_info["FavUsers"]       = FavoriteManager::getInstance()->getCountFavsUsers();
			l_stat_info["Threads"]        = Thread::getThreadsCount();
		}
		// ���������� �� �������� ������
		{
			static string g_first_time = Util::formatDigitalClock(time(nullptr));
			Json::Value& l_time_info   = l_info["Time"];
			l_time_info["Start"]       = g_first_time;
			l_time_info["Current"]     = Util::formatDigitalClock(time(nullptr));
			static bool g_is_first = false;
#ifndef _DEBUG
			if(!g_is_first)
#endif
			{
				g_is_first = true;
				Json::Value l_error_info;
				{
				// �������� ������
				auto appendError = [&l_error_info](const wchar_t* p_reg_value, const char* p_json_key)
				{
					const string l_reg_value = Util::getRegistryValueString(p_reg_value);
					if(!l_reg_value.empty())
					{
						Util::deleteRegistryValue(p_reg_value); // TODO - ������� ����� ����� ������ ���� ������ ������
						l_error_info[p_json_key] = l_reg_value;
				}
				};
				appendError(FLYLINKDC_REGISTRY_MEDIAINFO_CRASH_KEY,"MediaCrash");
				appendError(FLYLINKDC_REGISTRY_MEDIAINFO_FREEZE_KEY,"MediaFreeze");
				appendError(FLYLINKDC_REGISTRY_SQLITE_ERROR,"SQLite");
				appendError(FLYLINKDC_REGISTRY_LEVELDB_ERROR,"LevelDB");
				}
				if(!l_error_info.isNull())
				{
					l_info["Error"] = l_error_info;
				}
				// ����������
				l_time_info["StartGUI"]   = Util::toString(g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_GUI]);
				l_time_info["StartCore"]  = Util::toString(g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_CORE]);
				// ������� ���������� ������� ����������
				try
				{
				const string l_marker_file_name = Util::getConfigPath() + FLY_SHUTDOWN_FILE_MARKER_NAME;
					{
						File l_file (l_marker_file_name, File::READ, File::OPEN);
						const StringTokenizer<string> l_markers(l_file.read(), ',');
						dcassert(l_markers.getTokens().size() == 2);
						const auto& l_token = l_markers.getTokens();
						if(l_token.size() == 2)
						{
							l_time_info["PrevShutdownCore"] = l_token[0];
							l_time_info["PrevShutdownGUI"]  = l_token[1];
						}
					}
				File::deleteFile(l_marker_file_name);
				}
				catch (const FileException&)
				{
				}
			}
		}
#ifdef IRAINMAN_INCLUDE_GDI_OLE
		if(CGDIImage::g_AnimationDeathDetectCount || CGDIImage::g_AnimationCount || CGDIImage::g_AnimationCountMax)
		{
		 Json::Value& l_debug_info = l_info["Debug"];
		 if(CGDIImage::g_AnimationDeathDetectCount)
	     {
			  l_debug_info["AnimationDeathDetectCount"] = Util::toString(CGDIImage::g_AnimationDeathDetectCount);
		 }
		 if(CGDIImage::g_AnimationCount || CGDIImage::g_AnimationCountMax)
		 {
			  l_debug_info["AnimationCount"] = Util::toString(CGDIImage::g_AnimationCount);
			  l_debug_info["AnimationCountMax"] = Util::toString(CGDIImage::g_AnimationCountMax);
		 }
		}
#endif // IRAINMAN_INCLUDE_GDI_OLE
		// ������� ���������
		{
			Json::Value& l_net_info = l_info["Net"];
			if(BOOLSETTING(AUTO_DETECT_CONNECTION))
			{
			 l_net_info["AutoDetect"] = "1";
			}
			l_net_info["TypeConnect"] = SETTING(INCOMING_CONNECTIONS);
			if(!g_fly_server_stat.m_upnp_status.empty())
			{
			 l_net_info["UPNPStatus"]  = g_fly_server_stat.m_upnp_status;
			}
			if(!g_fly_server_stat.m_upnp_router_name.empty())
			{
			 l_net_info["Router"]      = g_fly_server_stat.m_upnp_router_name;
			}
		}
		const std::string l_post_query = l_info.toStyledString();
		bool l_is_send = false;
		bool l_is_error = false;
    if(l_is_flush_error == false) // ���� �� ������� �������� ������ ��� - ��� �����.
    {
		if(BOOLSETTING(USE_FLY_SERVER_STATICTICS_SEND) && p_is_sync_run == false)
		{
		  postQuery(true,true,false,false,false,"fly-stat",l_post_query,l_is_send,l_is_error,500);
		}
    }
    else
    {
        l_log.step("Skip stat-POST (internet error...)");
    }
		if(!l_is_send || p_is_sync_run)
			           // ���� �� ������� ��������� ��� ���������/��������. 
			           // �������� ����� �������� (����� � ������� ��������� ��������� �� �������)
		{
		if(BOOLSETTING(USE_FLY_SERVER_STATICTICS_SEND))
		  {
			CFlylinkDBManager::getInstance()->push_json_statistic(l_post_query);
		  }
        }
		return l_is_flush_error;
}
#endif // FLYLINKDC_USE_GATHER_STATISTICS
//======================================================================================================
string CFlyServerAdapter::CFlyServerJSON::postQuery(bool p_is_set, 
													bool p_is_stat_server,
													bool p_is_test_port_server,
													bool p_is_disable_zlib_in,
													bool p_is_disable_zlib_out,
													const char* p_query, 
													const string& p_body,
													bool& p_is_send,
													bool& p_is_error,
													DWORD p_time_out /*= 0*/,
                          const CServerItem* p_server /*= nullptr */)
{
//  Thread::ConditionLockerWithSpin l(g_running);
    //dcassert(g_running == 1);
	p_is_send = false;
	p_is_error = false;
	dcassert(!p_body.empty());
	CServerItem l_Server =	p_server ? *p_server : p_is_test_port_server ? CFlyServerConfig::getTestPortServer() :
		                       p_is_stat_server ? CFlyServerConfig::getStatServer() : 
	                        CFlyServerConfig::getRandomMirrorServer(p_is_set);
  dcassert(!l_Server.getIp().empty());
	if(!g_debug_fly_server_url.empty())
	{
		l_Server.setIp(g_debug_fly_server_url); // ����������� ����� ����-������� ��� ���� �������� �� ����������
	}
	const string l_log_marker = "[" + l_Server.getServerAndPort() + "]";
	CFlyLog l_fly_server_log(l_log_marker);
#ifdef PPA_INCLUDE_IPGUARD
	if (BOOLSETTING(ENABLE_IPGUARD) && IpGuard::isValidInstance()) 
	{
		string l_reason;
		if (IpGuard::getInstance()->check(l_Server.getIp(), l_reason))
		{
			l_fly_server_log.step(" (" + l_Server.getIp() + "): IPGuard: " + l_reason);
			return Util::emptyString;
		}
	}
#endif
	string l_result_query;
	//static const char g_hdrs[]		= "Content-Type: application/x-www-form-urlencoded"; // TODO - ��� �����?
	//static const size_t g_hdrs_len   = strlen(g_hdrs); // ����� �������� �� (sizeof(g_hdrs)-1) ...
	// static LPCSTR g_accept[2]	= {"*/*", NULL};
	std::vector<uint8_t> l_post_compress_query;
	string l_log_string;
	if(g_fly_server_config.getZlibCompressLevel() > Z_NO_COMPRESSION) // ��������� ������ �������?
	{
		if(p_is_disable_zlib_in == false)
		{
		unsigned long l_dest_length = compressBound(p_body.length()) + 2;
		l_post_compress_query.resize(l_dest_length);
		const int l_zlib_result = compress2(l_post_compress_query.data(), &l_dest_length,
	                         (uint8_t*)p_body.data(), 
							 p_body.length(), g_fly_server_config.getZlibCompressLevel()); 
                             // �� ������� ���� ���� �� ��������� - �������� ������. 
                             // ���� ������������ ����� ������ CPU ������� ����� ��������� ����� ������ (�� ������������)
		if (l_zlib_result == Z_OK)
		{
			if(l_dest_length < p_body.length()) // ������ ������ ����� ����� ������ - ������������ �� ������.
			{
				l_post_compress_query.resize(l_dest_length); // TODO - ����� �� ���� ��������?
				l_log_string = "Request:  " + Util::toString(p_body.length()) + " / " + Util::toString(l_dest_length);				
			}
			else
			{
				l_post_compress_query.clear();
			}
		}
		else
		{
			l_post_compress_query.clear();
			l_fly_server_log.step("Error zlib: =  " + Util::toString(l_zlib_result));
		}
	}
	}
	const bool l_is_zlib = !l_post_compress_query.empty();
// ��������
	CInternetHandle hSession(InternetOpen(g_full_user_agent.c_str(),INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0));
	DWORD l_timeOut = p_time_out ? p_time_out : CFlyServerConfig::g_winet_connect_timeout;
	if(l_timeOut < 500)
	   l_timeOut = 1000;
	if(!InternetSetOption(hSession, INTERNET_OPTION_CONNECT_TIMEOUT, &l_timeOut, sizeof(l_timeOut)))
	{
		l_fly_server_log.step("Error InternetSetOption INTERNET_OPTION_CONNECT_TIMEOUT: " + Util::translateError());
		p_is_error = true;
	}
	InternetSetOption(hSession, INTERNET_OPTION_RECEIVE_TIMEOUT, &CFlyServerConfig::g_winet_receive_timeout, sizeof(CFlyServerConfig::g_winet_receive_timeout));
	InternetSetOption(hSession, INTERNET_OPTION_SEND_TIMEOUT, &CFlyServerConfig::g_winet_send_timeout, sizeof(CFlyServerConfig::g_winet_send_timeout));
	if(hSession)
	{
		DWORD dwFlags = 0; //INTERNET_FLAG_NO_COOKIES|INTERNET_FLAG_RELOAD|INTERNET_FLAG_NO_CACHE_WRITE|INTERNET_FLAG_PRAGMA_NOCACHE;
		CInternetHandle hConnect(InternetConnectA(hSession, l_Server.getIp().c_str(),l_Server.getPort(), NULL, NULL, INTERNET_SERVICE_HTTP, dwFlags, NULL));
		if(hConnect)
		{
			CInternetHandle hRequest(HttpOpenRequestA(hConnect, "POST", p_query , NULL, NULL, NULL /*g_accept*/, 0, NULL));
			if(hRequest)
			{
				string l_fly_header;
				if(l_Server.getTimeResponse() > CFlyServerConfig::g_winet_min_response_time_for_log)
				{
					l_fly_header = "X-fly-response: " + Util::toString(l_Server.getTimeResponse());
				}
				if(HttpSendRequestA(hRequest, 
					    l_fly_header.length() ? l_fly_header.c_str() : nullptr,
					    l_fly_header.length(),  
						l_is_zlib ? reinterpret_cast<LPVOID>(l_post_compress_query.data()) : LPVOID(p_body.data()), 
						l_is_zlib ? l_post_compress_query.size() : p_body.size()))
				{
					DWORD l_dwBytesAvailable = 0;
					std::vector<char> l_zlib_blob;
					std::vector<unsigned char> l_MessageBody;
					while (InternetQueryDataAvailable(hRequest, &l_dwBytesAvailable, 0, 0))
					{
						if(l_dwBytesAvailable == 0)
							break;
#ifdef _DEBUG
						l_fly_server_log.step(" InternetQueryDataAvailable dwBytesAvailable = " + Util::toString(l_dwBytesAvailable));
#endif
						l_MessageBody.resize(l_dwBytesAvailable + 1);
						DWORD dwBytesRead = 0;
						const BOOL bResult = InternetReadFile(hRequest, l_MessageBody.data(),l_dwBytesAvailable, &dwBytesRead);
						if (!bResult)
						{
							l_fly_server_log.step(" InternetReadFile error " + Util::translateError());
							break;
						}
						if (dwBytesRead == 0)
							break;
						l_MessageBody[dwBytesRead] = 0;
						const auto l_cur_size = l_zlib_blob.size();
						l_zlib_blob.resize(l_cur_size + dwBytesRead);
						memcpy(l_zlib_blob.data() + l_cur_size, l_MessageBody.data(), dwBytesRead);
					}
#ifdef _DEBUG
/*
std::string l_hex_dump;
				for(unsigned long i=0;i<l_zlib_blob.size(); ++i)
				{
					if(i % 10 == 0)
						l_hex_dump += "\r\n";
					char b[7] = {0};
					sprintf(b, "%#x ", (unsigned char)l_zlib_blob[i] & 0xFF);
					l_hex_dump += b;
				}
					l_fly_server_log.step( "l_hex_dump = " + l_hex_dump);
*/
#endif
					// TODO �������� � ��������� ������� � ��������� � ������� DHT � ������ ��������� ����-������. ��� ���������� ���-�� ���������
					// TODO ����. ����� ����������� ������������ � �� �������� 10-���
					if(l_zlib_blob.size())
					{
					if(p_is_disable_zlib_out == true)
					{
						const auto l_cur_size = l_zlib_blob.size();
						l_zlib_blob.resize(l_cur_size + 1);
						l_zlib_blob[l_cur_size] = 0;
						l_result_query = (const char*) l_zlib_blob.data();
						//l_decompress.resize(l_zlib_blob.size());
						//memcpy(l_decompress.data(), l_zlib_blob.data(), l_decompress.size());
					}
					else
					{
					std::vector<unsigned char> l_decompress; // TODO ��������� �����������
					unsigned long l_decompress_size = l_zlib_blob.size() * 10;
					l_decompress.resize(l_decompress_size);
					while(true)
					{
					    const int l_un_compress_result = uncompress(l_decompress.data(), &l_decompress_size, (uint8_t*)l_zlib_blob.data(), l_zlib_blob.size());
						l_fly_server_log.step(l_log_string + ", Response: " + Util::toString(l_zlib_blob.size()) + " / " +  Util::toString(l_decompress_size));
						if (l_un_compress_result == Z_BUF_ERROR)
						{
							l_decompress_size *= 2;
							l_decompress.resize(l_decompress_size);
							continue;
						}

						if (l_un_compress_result == Z_OK)
						{
							l_result_query = (const char*) l_decompress.data();
						}
						else
						{
							l_fly_server_log.step(" InternetReadFile - uncompress error! error = " + Util::toString(l_un_compress_result));
							dcassert(l_un_compress_result == Z_OK);
						}
						break;
					}
					}
				    }
					p_is_send = true;
#ifdef _DEBUG
					l_fly_server_log.step(" InternetReadFile Ok! size = " + Util::toString(l_result_query.size()));
#endif
				}
				else
				{
					l_fly_server_log.step("HttpSendRequest error " + Util::translateError());
					p_is_error = true;
				}
			}
			else
			{
				l_fly_server_log.step("HttpOpenRequest error " + Util::translateError());
				p_is_error = true;
			}
		}
		else
		{
			l_fly_server_log.step("InternetConnect error " + Util::translateError());
			p_is_error = true;
		}
	}
	else
	{
		l_fly_server_log.step("InternetOpen error " + Util::translateError());
		p_is_error = true;
	}
	l_Server.setTimeResponse(l_fly_server_log.calcSumTime());
	return l_result_query;
}
//======================================================================================================
void CFlyServerAdapter::CFlyServerJSON::addDownloadCounter(const CFlyTTHKey& p_file)
{
	Lock l(g_cs_fly_server);
	g_download_counter.push_back(p_file);
}
//======================================================================================================
bool CFlyServerAdapter::CFlyServerJSON::sendDownloadCounter()
{
	bool l_is_error = false;
	if(!g_download_counter.empty())
	{
	CFlyTTHKeyArray l_copy_array;
	{
		Lock l(g_cs_fly_server);
		l_copy_array.swap(g_download_counter);
	}	
  l_is_error = login();
  if(l_is_error == false)
  {
	Json::Value  l_root;   
	Json::Value& l_arrays = l_root["array"];
	int l_count = 0;
	for(auto i = l_copy_array.cbegin(); i != l_copy_array.cend(); ++i)
	{
	 Json::Value& l_array_item = l_arrays[l_count++];
	 l_array_item["tth"]  = i->m_tth.toBase32();
	 l_array_item["size"] = Util::toString(i->m_file_size);
	}
    const std::string l_post_query = l_root.toStyledString();
    bool l_is_send = false;
    if(l_count)
     {
      postQuery(true, false, false, false, true,"fly-download",l_post_query,l_is_send,l_is_error,500);
     }
	}
  if(l_is_error)
  {
      dcassert(0); // TODO - ��������� � ����
  }
	}
   // TODO - ������� ���������� � ���� ����� ��������� l_copy_array � ����.
   // ����� �� �������� ��������.
   return l_is_error;
}
//======================================================================================================
string CFlyServerAdapter::CFlyServerJSON::connect(const CFlyServerKeyArray& p_fileInfoArray, bool p_is_fly_set_query, bool p_is_ext_info_for_single_file /* = false*/)
{
	dcassert(!p_fileInfoArray.empty());
	Thread::ConditionLockerWithSpin l(g_running);
  bool l_is_error = login(); // �������� ���� �� � �������
  string l_result_query;
  if(l_is_error == false)
  {
	// CFlyLog l_log("[flylinkdc-server]",true);
	Json::Value  l_root;   
	Json::Value& l_arrays = l_root["array"];
	l_root["header"]["ID"] = g_fly_server_id;
	int  l_count = 0;
	int  l_count_only_ext_info = 0;
	int  l_count_only_counter  = 0;
	int  l_count_cache  = 0;
	bool is_all_file_only_ext_info = false;
	bool is_all_file_only_counter  = false;
	if(!p_is_fly_set_query) // ��� get-������� ��� ������ ����� ������ ����������� ����?
	{
	 for(auto i = p_fileInfoArray.cbegin(); i != p_fileInfoArray.cend(); ++i)
	 {
	  if(i->m_only_ext_info)
		++l_count_only_ext_info;
	  if(i->m_only_counter)
		++l_count_only_counter;
	  if(i->m_is_cache)
	    ++l_count_cache; 
	 }
	 if(l_count_only_counter == p_fileInfoArray.size())
	 {
	   is_all_file_only_counter = true;
	   l_root["only_counter"] = 1; // �������� ������� � ������ ����� �� ���������� ������ ����.
	 }
	 if(l_count_only_counter > 0 && l_count_only_counter != p_fileInfoArray.size())
	 {
	   l_root["different_counter"] = 1; 
	 }
	 if(l_count_cache)
	 {
	   l_root["cache"] = l_count_cache;  // ��� ���������� TODO - ����� ������
	 }
	 if(is_all_file_only_counter == false) // ���� �� ������ ������ �������� - ��������� ������� �� ����������� ����
	 {
	  if(l_count_only_ext_info == p_fileInfoArray.size()) // ������ ���� ������ ������ ��� �������� ��������
	  {
	   is_all_file_only_ext_info = true;
	   l_root["only_ext_info"] = 1; // �������� ������� � ������ ����� �� ���������� ������ ����.
	  }
	 if(l_count_only_ext_info > 0 && l_count_only_ext_info != p_fileInfoArray.size())
	  {
	   l_root["different_ext_info"] = 1; // �������� ������� ��������� �������� ������� � ��������� 
	                                     // ����� �� ������� ������� �� ������ ������ ����� ��������� ��� ������� �����.
	  }
	 }
	}
	for(auto i = p_fileInfoArray.cbegin(); i != p_fileInfoArray.cend(); ++i)
	{
	// �������� ��������� JSON ����� ��� �������� �� ������.
	const string l_tth_base32 = i->m_tth.toBase32();
	dcassert(i->m_file_size && !l_tth_base32.empty());
	if(!i->m_file_size || l_tth_base32.empty())
	{
		LogManager::getInstance()->message("Error !i->m_file_size || l_tth_base32.empty(). i->m_file_size = " +
			        Util::toString(i->m_file_size) 
					+ " l_tth_base32 = " + l_tth_base32 /*+ " i->m_file_name = " + i->m_file_name */);
		continue;
	}
	Json::Value& l_array_item = l_arrays[l_count++];
	l_array_item["tth"]  = l_tth_base32;
	l_array_item["size"] = Util::toString(i->m_file_size);
	 // TODO - l_array_item["name"] = i->m_file_name;  ��� ���� �� �������. ��� ������ ������
	 /* ������� ����� � ������� ����/����� ���-�� ���������
	 if(i->m_hit)
	  l_array_item["hit"] = i->m_hit;
	 if(i->m_time_hash)
	 {
		l_array_item["time_hash"] = i->m_time_hash;
		l_array_item["time_file"] = i->m_time_file;
	 }
	 */
	if (p_is_fly_set_query == false)
	{
		if(is_all_file_only_ext_info == false && i->m_only_ext_info == true)
	        l_array_item["only_ext_info"] = 1;
		if(is_all_file_only_counter == false && i->m_only_counter == true)
	        l_array_item["only_counter"] = 1;
	}
	else
	{
		Json::Value& l_mediaInfo = l_array_item["media"];
		if(!i->m_media.m_audio.empty())
		  l_mediaInfo["fly_audio"] = i->m_media.m_audio;
		if(!i->m_media.m_video.empty())
		  l_mediaInfo["fly_video"] = i->m_media.m_video;
		if(i->m_media.m_bitrate)
		  l_mediaInfo["fly_audio_br"] = i->m_media.m_bitrate;
		if(i->m_media.m_mediaX && i->m_media.m_mediaY)
		{
		  l_mediaInfo["fly_xy"] = i->m_media.getXY();
		}

	if (i->m_media.isMediaAttrExists())
	{
				Json::Value& l_mediaInfo_ext = l_array_item["media-ext"];
				Json::Value* l_ptr_mediaInfo_ext_general = nullptr;
				Json::Value* l_ptr_mediaInfo_ext_audio   = nullptr;
				Json::Value* l_ptr_mediaInfo_ext_video   = nullptr;
				int l_count_audio_channel = 0;
				int l_last_channel = 0;
				for(auto j = i->m_media.m_ext_array.cbegin(); j!= i->m_media.m_ext_array.cend(); ++j)
				{
						if(j->m_stream_type == MediaInfoLib::Stream_Audio)
					{
						if(j->m_channel != l_last_channel)
						{
							l_last_channel = j->m_channel;
							++l_count_audio_channel;
						}
					}
				}
				boost::unordered_map<int,Json::Value*> l_cache_channel;
				for(auto j = i->m_media.m_ext_array.cbegin(); j!= i->m_media.m_ext_array.cend(); ++j)
				{
					if(CFlyServerConfig::isSupportTag(j->m_param))
					{
					switch(j->m_stream_type)
					{
					case MediaInfoLib::Stream_General:
						{
							if(!l_ptr_mediaInfo_ext_general)
								l_ptr_mediaInfo_ext_general = &l_mediaInfo_ext["general"];
							Json::Value& l_info = *l_ptr_mediaInfo_ext_general;
							l_info[j->m_param] = j->m_value;
						}
						break;
					case MediaInfoLib::Stream_Video:
						{
							if(!l_ptr_mediaInfo_ext_video)
								l_ptr_mediaInfo_ext_video = &l_mediaInfo_ext["video"];
							Json::Value& l_info = *l_ptr_mediaInfo_ext_video;
							l_info[j->m_param] = j->m_value;
						}
						break;
					case MediaInfoLib::Stream_Audio:
						{
							if(!l_ptr_mediaInfo_ext_audio)
								l_ptr_mediaInfo_ext_audio = &l_mediaInfo_ext["audio"];
							Json::Value& l_info = *l_ptr_mediaInfo_ext_audio;
							if(l_count_audio_channel == 0)
							  {
								  l_info[j->m_param] = j->m_value;
							  }
							else
							  {
								  // ����������, � ����� ������ ������ ������ ������� ����������.
								  uint8_t l_channel_num = j->m_channel;
								  const auto l_ch_item = l_cache_channel.find(l_channel_num);
								  Json::Value* l_channel_info = 0;
								  if(l_ch_item == l_cache_channel.end())
								  {
									  const string l_channel_id = "channel-" + (l_channel_num != CFlyMediaInfo::ExtItem::channel_all ? Util::toString(l_channel_num) : string("all"));
									  l_channel_info = &l_info[l_channel_id];
									  l_cache_channel[l_channel_num] =  l_channel_info;
								  }
								  else
								  {
										  l_channel_info = l_ch_item->second;
								  }
								  (*l_channel_info)[j->m_param] = j->m_value;
								  }
						}
						break;
					}
					}
				}
			}
	  }
	}
// string l_tmp;
 if (l_count > 0)  // ���� ��� ���������� �� ������?
 {
#define FLYLINKDC_USE_HTTP_SERVER
#ifdef FLYLINKDC_USE_HTTP_SERVER
   const std::string l_post_query = l_root.toStyledString();
   bool l_is_send = false;
   if(l_is_error == false)
   {
   l_result_query = postQuery(p_is_fly_set_query, false, false, false, false, p_is_fly_set_query? "fly-set" : p_is_ext_info_for_single_file ? "fly-zget-full" : "fly-zget",l_post_query,l_is_send,l_is_error);
   }
#endif

// #define FLYLINKDC_USE_SOCKET
// #ifndef FLYLINKDC_USE_SOCKET
//  ::MessageBoxA(NULL,outputConfig.c_str(),"json",MB_OK | MB_ICONINFORMATION);
// #endif
#ifdef FLYLINKDC_USE_SOCKET
	unique_ptr<Socket> l_socket(new Socket);
	try
	{
	l_socket->create(Socket::TYPE_TCP);
	l_socket->setBlocking(true);
//#define FLYLINKDC_USE_MEDIAINFO_SERVER_RESOLVE
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER_RESOLVE
	const string l_ip = Socket::resolve("flylink.no-ip.org");
	l_log.step("Socket::resolve(flylink.no-ip.org) = ip " + l_ip);
#else
	const string l_ip = g_fly_server_config.m_ip;
#endif
	l_socket->connect(l_ip,g_fly_server_config.m_port); // 
	l_log.step("write");
	const size_t l_size = l_socket->writeAll(outputConfig.c_str(), outputConfig.size(), 500);
	if(l_size != outputConfig.size())
	 {
		 l_log.step("l_socket->write(outputConfig) != outputConfig.size() l_size = " + Util::toString(l_size));
		 return outputConfig;
	 }
	else
	{
		l_log.step("write-OK");
		vector<char> l_buf(64000);
		l_log.step("read");
		l_socket->readAll(&l_buf[0],l_buf.size(),500);
		l_log.step("read-OK");
	}
	l_log.step(outputConfig);
	}
	catch(const SocketException& e)
	{
	l_log.step("Socket error" + e.getError());
	}
#endif
	}
 }
 else
 {
     dcassert(0);
 }
	return l_result_query;
}
//======================================================================================================
string CFlyServerInfo::getMediaInfoAsText(const TTHValue& p_tth,int64_t p_file_size)
{
	CFlyServerKeyArray l_get_array;
	CFlyServerKey l_info(p_tth, p_file_size);
	l_info.m_only_ext_info = true; // �������� � ������� ������ �����������.
	l_get_array.push_back(l_info);
	const string l_json_result = CFlyServerAdapter::CFlyServerJSON::connect(l_get_array, false, true);
	string l_Infrom;
	Json::Value l_root;
	Json::Reader l_reader(Json::Features::strictMode());
	const bool l_parsingSuccessful = l_reader.parse(l_json_result, l_root);
	if (!l_parsingSuccessful && !l_json_result.empty())
	{
		l_Infrom = l_json_result;
		LogManager::getInstance()->message("Failed to parse json configuration: l_json_result = " + l_json_result);
	}
	else
	{
		const Json::Value& l_arrays = l_root["array"];
		dcassert(l_arrays.size() == 0 || l_arrays.size() == 1);
		const Json::Value& l_cur_item_in = l_arrays[0U];
		const Json::Value& l_attrs_media_ext = l_cur_item_in["media-ext"];
		const Json::Value& l_attrs_general   = l_attrs_media_ext["general"];
		l_Infrom = l_attrs_general["Inform"].asString();
		const Json::Value& l_attrs_video   = l_attrs_media_ext["video"];
		const string l_InfromVideo = l_attrs_video["Inform"].asString();
		if (!l_InfromVideo.empty())
		{
			l_Infrom += "\r\nVideo ";
			l_Infrom += l_attrs_video["Inform"].asString();
		}
		const Json::Value& l_attrs_audio   = l_attrs_media_ext["audio"];
		int i = 0;
		for (;; ++i)
		{
			const string l_channel_id = "channel-" + Util::toString(i);
			const Json::Value& l_attrs_channel   = l_attrs_audio[l_channel_id];
			const string& l_channel_value = l_attrs_channel["Inform"].asString();
			if (l_channel_value.empty())
				break;
			else
			{
				if (!l_channel_value.empty())
				{
					l_Infrom += "\r\nAudio ";
					l_Infrom += l_channel_value + ":\r\n";
				}
			}
		}
		if (i == 0) // ������� ���?
		{
			const string l_audio = l_attrs_audio["Inform"].asString();
			if (!l_audio.empty())
			{
				l_Infrom += "\r\nAudio ";
				l_Infrom += l_audio;
			}
		}
	}
	return l_Infrom;
}
//=========================================================================================
string g_cur_mediainfo_file;
string g_cur_mediainfo_file_tth;
//=========================================================================================
static void getExtMediaInfo(const string& p_file_ext_wo_dot,
                            int64_t p_size,
                            MediaInfoLib::MediaInfo& p_media_info_dll,
                            MediaInfoLib::stream_t p_stream_type,
                            CFlyMediaInfo& p_media,
                            bool p_compress_channel_attr)
{
	const ZtringListList l_info = MediaInfoLib::Config.Info_Get(p_stream_type);
	if (const size_t l_count = p_media_info_dll.Count_Get(p_stream_type))
	{
		int l_count_audio_channel = p_stream_type == MediaInfoLib::Stream_Audio ? l_count : 0; // ����� ������� ������� ������ ��� Audio
		for (auto i = l_info.cbegin() ; i != l_info.cend(); ++i)
		{
			const auto l_param_name = i->Read(0);
			for (size_t j = 0; j < l_count; ++j)
			{
				const auto l_value = p_media_info_dll.Get(p_stream_type, j, l_param_name);
				if (!l_value.empty())
				{
					const string l_str_param_name = Text::fromT(l_param_name);
					if (CFlyServerConfig::isSupportTag(l_str_param_name)) // TODO Fix fromT
					{
						CFlyMediaInfo::ExtItem l_ext_item;
						l_ext_item.m_stream_type = p_stream_type;
						l_ext_item.m_channel = j;
						l_ext_item.m_param = l_str_param_name;
						l_ext_item.m_value = Text::fromT(l_value);
						// Inform - ������� �������� - ��������� ��� ��� �������� ������ ������� � ��������.
						if (l_str_param_name.compare(0, 6, "Inform", 6) == 0)
						{
							g_fly_server_config.ConvertInform(l_ext_item.m_value);
						}
						p_media.m_ext_array.push_back(l_ext_item);
					}
				}
			}
		}
		if (p_compress_channel_attr && l_count_audio_channel) // ���� �����-������� ���������. ������������� ���������� ������
		{
			// ������� ������ ������� ���� �� ��������� � �������� ��������� ���������� ��� ����.
			// ���� �������� ��������� ����-���������
			// ���������� �������� ��� ������� ������� ������� ����� �� ������ � ����.
			// TODO - �������������� � ���������� �������� ������ ������� �� �������
			std::map <std::string, std::pair< std::string, int> > l_channel_dup_filter;
			// �������� MI - ���� - { �������� ��������� + ������� ��������� ��������� ��� �������� }
			for (auto j = p_media.m_ext_array.cbegin(); j != p_media.m_ext_array.cend(); ++j)
				// TODO [!] ���� ������ ����� ��������� ����� ��������� ��������� ����������.
				// ���� ��� �������� ������� ��� �� Audio - ��������� ������� �����
			{
				dcassert(j->m_stream_type == MediaInfoLib::Stream_Audio);
				if (j->m_stream_type == MediaInfoLib::Stream_Audio)
				{
					auto& l_value = l_channel_dup_filter[j->m_param];
					if (l_value.first != j->m_value)
					{
						if (l_value.first.empty()) // ������ ��������?
						{
							l_value.first  = j->m_value;
						}
						l_value.second++;
					}
				}
			}
			// ��������� ���������� �� ���������.
			// ������� ����������� ������ ������� ��� ������������ ������ ��� ������ � ���� ������.
			for (auto k = p_media.m_ext_array.begin(); k != p_media.m_ext_array.end(); ++k)
			{
				dcassert(k->m_stream_type == MediaInfoLib::Stream_Audio);
				if (k->m_stream_type == MediaInfoLib::Stream_Audio)
				{
					auto l_channel_filter = l_channel_dup_filter.find(k->m_param);
					dcassert(l_channel_filter != l_channel_dup_filter.end());
					if (l_channel_filter->second.second == 1) // ��� ���� ������� �������� ��������� ����������?
					{
						k->m_channel  = CFlyMediaInfo::ExtItem::channel_all; // ����� ��� ������� ��������� ������ 255 (channel-All)
						l_channel_filter->second.second = -1; // ��������� ������� � -1 ����� �� ��������� ������� ������� ������
						continue;
					}
					if (l_channel_filter->second.second == -1) // �������� ��������� ��� ���������� � ����� ����� � ����� 255 �� ���������� ���������
						// � ������� ������ ���������� � �� ������ � ���� �� �����.
						// TODO - �������� �� vector-� ���� �� ������. ����� ��� ������ �������� �� ����������
					{
						k->m_is_delete = true;
					}
				}
			}
		}
	}
}
//======================================================================================================
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
//=========================================================================================
bool getMediaInfo(const string& p_name, CFlyMediaInfo& p_media, int64_t p_size, const TTHValue& p_tth, bool p_force /* = false*/)
{
	try
	{
		static MediaInfoLib::MediaInfo g_media_info_dll;
		if (p_size < SETTING(MIN_MEDIAINFO_SIZE) * 1024 * 1024) // TODO: p_size?
			return false;
		const string l_file_ext = Text::toLower(Util::getFileExtWithoutDot(p_name));
		if (!CFlyServerConfig::isMediainfoExt(l_file_ext))
			return false;
		char l_size[22];
		l_size[0] = 0;
		_snprintf(l_size, _countof(l_size), "%I64d", p_size);
		g_cur_mediainfo_file_tth = p_tth.toBase32();
		g_cur_mediainfo_file = p_name + "\r\n TTH = " + g_cur_mediainfo_file_tth + "\r\n File size = " + string(l_size);
		if (g_media_info_dll.Open(Text::toT(File::formatPath(p_name))))
		{
			// const bool l_is_media_info_fly_server = g_fly_server_config.isSupportFile(l_file_ext, p_size);
			// if (l_is_media_info_fly_server)
			// �������� �������� ��������� ������ - ����� ����� ��������� �������������� ���������� �� ���� ������� � �������.
			{
				// TODO - ���������� ����� �� ������� ������� ������ (����� ������� ��-�� ��������� p_compress_channel_attr)
				getExtMediaInfo(l_file_ext, p_size, g_media_info_dll, MediaInfoLib::Stream_Audio, p_media, true); // ������ ��������� � �������
				getExtMediaInfo(l_file_ext, p_size, g_media_info_dll, MediaInfoLib::Stream_General, p_media, false);
				getExtMediaInfo(l_file_ext, p_size, g_media_info_dll, MediaInfoLib::Stream_Video, p_media, false);
// ��� ���� ������
//          getExtMediaInfo(l_file_ext, p_size, g_media_info_dll,MediaInfoLib::Stream_Text,p_media);
//			getExtMediaInfo(l_file_ext, p_size, g_media_info_dll,MediaInfoLib::Stream_Chapters,p_media);
//			getExtMediaInfo(l_file_ext, p_size, g_media_info_dll,MediaInfoLib::Stream_Image,p_media);
//			getExtMediaInfo(l_file_ext, p_size, g_media_info_dll,MediaInfoLib::Stream_Menu,p_media);
			}
			const size_t audioCount = g_media_info_dll.Count_Get(MediaInfoLib::Stream_Audio);
			p_media.m_bitrate  = 0;
			boost::unordered_map<string, uint16_t> l_audio_dup_filter;
			// AC-3, 5.1, 448 Kbps | AC-3, 5.1, 640 Kbps | TrueHD / AC-3, 5.1, 640 Kbps | AC-3, 5.1, 448 Kbps | AC-3, 5.1, 448 Kbps | AC-3, 5.1, 448 Kbps | AC-3, 5.1, 448 Kbps | AC-3, 5.1, 448 Kbps | AC-3, 5.1, 448 Kbps"
			// ���������� �
			// AC-3, 5.1, 640 Kbps | TrueHD / AC-3, 5.1, 640 Kbps | AC-3, 5.1, 448 Kbps (x7)
			// dcassert(audioCount);
			for (size_t i = 0; i < audioCount; i++)
			{
				const wstring l_sinfo = g_media_info_dll.Get(MediaInfoLib::Stream_Audio, i, _T("BitRate"));
				uint16_t bitRate = (Util::toFloat(Text::fromT(l_sinfo)) / 1000.0 + 0.5);
				if (bitRate > p_media.m_bitrate)
					p_media.m_bitrate = bitRate;
				wstring sFormat = g_media_info_dll.Get(MediaInfoLib::Stream_Audio, i, _T("Format"));
#if defined (SSA_REMOVE_NEEDLESS_WORDS_FROM_VIDEO_AUDIO_INFO)
				boost::replace_all(sFormat, _T(" Audio"), Util::emptyStringT);
#endif
				const wstring sBitRate = g_media_info_dll.Get(MediaInfoLib::Stream_Audio, i, _T("BitRate/String"));
				const wstring sChannelPos = g_media_info_dll.Get(MediaInfoLib::Stream_Audio, i, _T("ChannelPositions"));
				const uint16_t iChannels = Util::toInt(g_media_info_dll.Get(MediaInfoLib::Stream_Audio, i, _T("Channel(s)")));
				const auto l_pos = sChannelPos.find(_T("LFE"), 0);
				std::string sChannels;
				if (l_pos != string::npos)
				{
					sChannels = Util::toString(iChannels - 1) + ".1";
				}
				else
				{
					sChannels = Util::toString(iChannels) + ".0";
				}
				
				const wstring sLanguage = g_media_info_dll.Get(MediaInfoLib::Stream_Audio, i, _T("Language/String1"));
				std::string audioFormatString;
				if (!sFormat.empty() || !sBitRate.empty() || !sChannels.empty() || !sLanguage.empty())
				{
					if (!sFormat.empty())
					{
						audioFormatString += ' ';
						audioFormatString += Text::fromT(sFormat);
						audioFormatString += ',';
					}
					if (!sChannels.empty())
					{
						audioFormatString += ' ';
						audioFormatString += sChannels;
						audioFormatString += ',';
					}
					if (!sBitRate.empty())
					{
						audioFormatString += ' ';
						audioFormatString += Text::fromT(sBitRate);
						audioFormatString += ',';
					}
					if (!sLanguage.empty())
					{
						audioFormatString += ' ';
						audioFormatString += Text::fromT(sLanguage);
						audioFormatString += ',';
					}
					if (!audioFormatString.empty() && audioFormatString[audioFormatString.length() - 1] == ',')
					{
						audioFormatString = audioFormatString.substr(0, audioFormatString.length() - 1); // Remove last ,
					}
					l_audio_dup_filter[audioFormatString]++;
				}
			}
			std::string l_audio_all;
			std::string l_sep;
			for (auto k = l_audio_dup_filter.cbegin(); k != l_audio_dup_filter.cend(); ++k)
			{
				l_audio_all += l_sep;
				if (k->second == 1)
					l_audio_all += k->first;
				else
					l_audio_all += k->first + " (x" + Util::toString(k->second) + ")";
				l_sep = " |";
			}
			wstring l_width;
			wstring l_height;
#ifdef USE_MEDIAINFO_IMAGES
			l_width = g_media_info_dll.Get(MediaInfoLib::Stream_Image, 0, _T("Width"));
			l_height = g_media_info_dll.Get(MediaInfoLib::Stream_Image, 0, _T("Height"));
			if (l_width.empty() && l_height.empty())
#endif
			{
				l_width = g_media_info_dll.Get(MediaInfoLib::Stream_Video, 0, _T("Width"));
				if (!l_width.empty())
					l_height = g_media_info_dll.Get(MediaInfoLib::Stream_Video, 0, _T("Height"));
			}
			p_media.m_mediaX = Util::toInt(l_width);
			if (p_media.m_mediaX)
				p_media.m_mediaY = Util::toInt(l_height);
			else
				p_media.m_mediaY = 0;
				
			const wstring sDuration = g_media_info_dll.Get(MediaInfoLib::Stream_General, 0, _T("Duration/String"));
			if (!sDuration.empty() || !l_audio_all.empty())
			{
				string audioGeneral;
				if (!sDuration.empty())
				{
					audioGeneral += Text::fromT(sDuration) + " |";
				}
				p_media.m_audio = audioGeneral;
				// No Duration => No sound
				if (!l_audio_all.empty())
				{
					p_media.m_audio += l_audio_all;
				}
			}
			
			const size_t videoCount =  g_media_info_dll.Count_Get(MediaInfoLib::Stream_Video);
			if (videoCount > 0)
			{
				string videoString;
				for (size_t i = 0; i < videoCount; i++)
				{
					wstring sVFormat = g_media_info_dll.Get(MediaInfoLib::Stream_Video, i, _T("Format"));
#if defined (SSA_REMOVE_NEEDLESS_WORDS_FROM_VIDEO_AUDIO_INFO)
					boost::replace_all(sVFormat, _T(" Video"), Util::emptyStringT);
					boost::replace_all(sVFormat, _T(" Visual"), Util::emptyStringT);
#endif
					wstring sVBitrate = g_media_info_dll.Get(MediaInfoLib::Stream_Video, i, _T("BitRate/String"));
					wstring sVFrameRate = g_media_info_dll.Get(MediaInfoLib::Stream_Video, i, _T("FrameRate/String"));
					if (!sVFormat.empty() || !sVBitrate.empty() || !sVFrameRate.empty())
					{
					
						if (!sVFormat.empty())
						{
							videoString += Text::fromT(sVFormat);
							videoString += ", ";
						}
						if (!sVBitrate.empty())
						{
							videoString += Text::fromT(sVBitrate);
							videoString += ", ";
						}
						if (!sVFrameRate.empty())
						{
							videoString += Text::fromT(sVFrameRate);
							videoString += ", ";
						}
						videoString = videoString.substr(0, videoString.length() - 2); // Remove last ,
						videoString += " | ";
					}
				}
				
				if (videoString.length() > 3) // This is false only in theorical way.
					p_media.m_video = videoString.substr(0, videoString.length() - 3); // Remove last |
			}
			g_media_info_dll.Close();
		}
		g_cur_mediainfo_file_tth.clear();
		g_cur_mediainfo_file.clear();
		return true;
	}
	catch (std::exception& e)
	{
		const string l_error = g_cur_mediainfo_file + " TTH:" + p_tth.toBase32() + " error: " + string(e.what());
		CFlyServerAdapter::CFlyServerJSON::pushError(15, "error getmediainfo:" + l_error);
		Util::setRegistryValueString(FLYLINKDC_REGISTRY_MEDIAINFO_CRASH_KEY, Text::toT(l_error));
		LogManager::getInstance()->message("getMediaInfo: " + p_name + "TTH:" + p_tth.toBase32() + ' ' + STRING(ERROR_STRING) + ": " + string(e.what()));
		char l_buf[4000];
		l_buf[0] = 0;
		sprintf_s(l_buf, _countof(l_buf), CSTRING(ERROR_MEDIAINFO_SCAN), p_name.c_str(), e.what());
		::MessageBox(0, Text::toT(l_buf).c_str(), _T("Error mediainfo!"), MB_ICONERROR);
		return false;
	}
	catch (...)
	{
		// TODO ���� �� �������� ���� SEH - ����� ������ � ���������
		Util::setRegistryValueString(FLYLINKDC_REGISTRY_MEDIAINFO_CRASH_KEY, Text::toT(g_cur_mediainfo_file + " catch(...) "));
		CFlyServerAdapter::CFlyServerJSON::pushError(15, "error getmediainfo[2] " + g_cur_mediainfo_file + " TTH:" + p_tth.toBase32() + " catch(...)");
		throw;
	}
}
//=========================================================================================

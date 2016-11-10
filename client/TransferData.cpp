﻿//-----------------------------------------------------------------------------
//(c) 2007-2016 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------

#include "stdinc.h"

#include "ResourceManager.h"
#ifdef FLYLINKDC_USE_TORRENT
#include "libtorrent/torrent_status.hpp"
#endif
#include "TransferData.h"


#ifdef FLYLINKDC_USE_TORRENT
void TransferData::init(libtorrent::torrent_status const& s)
{
	//l_td.m_hinted_user = d->getHintedUser();
	//m_token = s.info_hash.to_string(); для токена используется m_sha1
	
	m_sha1 = s.info_hash;
	m_pos = 1;
	m_speed = s.download_payload_rate;
	m_actual = s.total_done;// d->getActual();
	m_second_left = 0;// d->getSecondsLeft();
	m_start = 0; // d->getStart();
	m_size = s.total_wanted; // ti->files().file_size(i);
	m_type = 0; // d->getType();
	m_path = s.save_path + s.name; // - путь к корню торрент-файла
	m_torrent_file_path = m_path; // s.save_path + l_file_path;
	m_num_seeds = s.num_seeds;
	m_num_peers = s.num_peers;
	m_is_torrent = true;
	m_is_seeding = s.state == libtorrent::torrent_status::seeding;
	//calc_percent();
	m_percent = s.progress_ppm / 10000;
	///l_td.m_status_string += _T("[Torrent] Peers:") + Util::toStringT(s.num_peers) + _T(" Seeds:") + Util::toStringT(s.num_seeds) + _T(" ");
	//l_td.m_status_string += Text::tformat(TSTRING(DOWNLOADED_BYTES), Util::formatBytesW(l_td.m_pos).c_str(),
	//  l_td.m_percent, l_td.get_elapsed(aTick).c_str());
	if (s.state == libtorrent::torrent_status::checking_files)
	{
		m_status_string += Text::tformat(TSTRING(CHECKED_BYTES), "", m_percent, "");
	}
	else
	{
		const tstring l_peer_seed = _T("[Torrent] Peers:") + Util::toStringT(s.num_peers) + _T(" Seeds:") + Util::toStringT(s.num_seeds) + _T(" ");
		if (s.state == libtorrent::torrent_status::seeding)
		{
			m_status_string = l_peer_seed + _T(" Seeding! Download: ") + Util::formatBytesW(s.total_download) + _T(" Upload: ") + Util::formatBytesW(s.total_upload);
		}
		else
		{
			m_status_string += l_peer_seed + Text::tformat(TSTRING(DOWNLOADED_BYTES), Util::formatBytesW(s.total_done).c_str(),
			                                               m_percent, get_elapsed(time(nullptr) - s.added_time).c_str());
		}
	}
}
#endif

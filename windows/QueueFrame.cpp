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

#include "stdafx.h"
#include "QueueFrame.h"
#include "SearchFrm.h"
#include "PrivateFrame.h"
#include "LineDlg.h"
#include "../client/ShareManager.h"
#include "../client/ClientManager.h"
#include "BarShader.h"
#include "ExMessageBox.h" // [+] InfinitySky. From Apex.
#include "MainFrm.h"

int QueueFrame::columnIndexes[] = { COLUMN_TARGET, COLUMN_STATUS, COLUMN_SEGMENTS, COLUMN_SIZE, COLUMN_PROGRESS, COLUMN_DOWNLOADED, COLUMN_PRIORITY,
                                    COLUMN_USERS, COLUMN_PATH,
                                    COLUMN_LOCAL_PATH, // http://code.google.com/p/flylinkdc/issues/detail?id=1261
                                    COLUMN_EXACT_SIZE, COLUMN_ERRORS, COLUMN_ADDED, COLUMN_TTH
                                  };

int QueueFrame::columnSizes[] = { 200, 300, 70, 75, 100, 120, 75, 200, 200, 200, 75, 200, 100, 125 };

static ResourceManager::Strings columnNames[] = { ResourceManager::FILENAME, ResourceManager::STATUS, ResourceManager::SEGMENTS, ResourceManager::SIZE,
                                                  ResourceManager::DOWNLOADED_PARTS, ResourceManager::DOWNLOADED,
                                                  ResourceManager::PRIORITY, ResourceManager::USERS, ResourceManager::PATH,
                                                  ResourceManager::LOCAL_PATH,
                                                  ResourceManager::EXACT_SIZE, ResourceManager::ERRORS,
                                                  ResourceManager::ADDED, ResourceManager::TTH_ROOT
                                                };

LRESULT QueueFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	showTree = BOOLSETTING(QUEUEFRAME_SHOW_TREE);
	
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	
	ctrlQueue.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                 WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_QUEUE);
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlQueue);
	
	ctrlDirs.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
	                TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP,
	                WS_EX_CLIENTEDGE, IDC_DIRECTORIES);
	                
	if (BOOLSETTING(USE_EXPLORER_THEME)
#ifdef FLYLINKDC_SUPPORT_WIN_2000
	        && CompatibilityManager::IsXPPlus()
#endif
	   )
		SetWindowTheme(ctrlDirs.m_hWnd, L"explorer", NULL);
		
	ctrlDirs.SetImageList(g_fileImage.getIconList(), TVSIL_NORMAL);
	ctrlQueue.SetImageList(g_fileImage.getIconList(), LVSIL_SMALL);
	
	m_nProportionalPos = SETTING(QUEUEFRAME_SPLIT);
	SetSplitterPanes(ctrlDirs.m_hWnd, ctrlQueue.m_hWnd);
	
	// Create listview columns
	WinUtil::splitTokens(columnIndexes, SETTING(QUEUEFRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(QUEUEFRAME_WIDTHS), COLUMN_LAST);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);
	
	for (uint8_t j = 0; j < COLUMN_LAST; j++)
	{
		int fmt = (j == COLUMN_SIZE || j == COLUMN_DOWNLOADED || j == COLUMN_EXACT_SIZE || j == COLUMN_SEGMENTS) ? LVCFMT_RIGHT : LVCFMT_LEFT;
		ctrlQueue.InsertColumn(j, TSTRING_I(columnNames[j]), fmt, columnSizes[j], j);
	}
	
	ctrlQueue.setColumnOrderArray(COLUMN_LAST, columnIndexes);
	ctrlQueue.setVisible(SETTING(QUEUEFRAME_VISIBLE));
	
	// ctrlQueue.setSortColumn(COLUMN_TARGET);
	ctrlQueue.setSortColumn(SETTING(QUEUE_COLUMNS_SORT));
	ctrlQueue.setAscending(BOOLSETTING(QUEUE_COLUMNS_SORT_ASC));
	
	SET_LIST_COLOR(ctrlQueue);
	ctrlQueue.setFlickerFree(Colors::bgBrush);
	
	ctrlDirs.SetBkColor(Colors::bgColor);
	ctrlDirs.SetTextColor(Colors::textColor);
	
	ctrlShowTree.Create(ctrlStatus.m_hWnd, rcDefault, _T("+/-"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	ctrlShowTree.SetButtonStyle(BS_AUTOCHECKBOX, false);
	ctrlShowTree.SetCheck(showTree);
	showTreeContainer.SubclassWindow(ctrlShowTree.m_hWnd);
	
	browseMenu.CreatePopupMenu();
	removeMenu.CreatePopupMenu();
	removeAllMenu.CreatePopupMenu();
	pmMenu.CreatePopupMenu();
	readdMenu.CreatePopupMenu();
	
	removeMenu.AppendMenu(MF_STRING, IDC_REMOVE_SOURCE, CTSTRING(ALL));
	removeMenu.AppendMenu(MF_SEPARATOR);
	
	readdMenu.AppendMenu(MF_STRING, IDC_READD, CTSTRING(ALL));
	readdMenu.AppendMenu(MF_SEPARATOR);
	
	addQueueList();
	QueueManager::getInstance()->addListener(this);
	
	SettingsManager::getInstance()->addListener(this);
	
	memzero(statusSizes, sizeof(statusSizes));
	statusSizes[0] = 16;
	ctrlStatus.SetParts(6, statusSizes);
	updateStatus();// [!]IRainman optimize QueueFrame
	create_timer(1000);
	bHandled = FALSE;
	return 1;
}

const tstring QueueFrame::QueueItemInfo::getText(int col) const
{
	// [-] if (!qi) return Util::emptyStringT; [-] IRainman fix: imposibru! Please report to me if the app crashing here.
	switch (col)
	{
		case COLUMN_TARGET:
			return Text::toT(Util::getFileName(getTarget())); // [!] IRainman fix done [13] https://www.box.net/shared/0lg6ozkidynjg7ezkhgz
		case COLUMN_STATUS:
		{
			SharedLock l(QueueItem::cs);
			if (isFinishedL())
			{
				return TSTRING(DOWNLOAD_FINISHED_IDLE);
			}
			const size_t l_online = QueueManager::countOnlineUsersL(qi); // [!] IRainman fix done 2012-04-29_13-46-19_NRAIMLXLGO4PGYESCVW76KIYPJCSLGLGP2LS2WY_712318D1_crash-stack-r501-x64-build-9869.dmp
			const size_t size = QueueManager::getSourcesCountL(qi);
			if (isWaitingL())
			{
				if (l_online > 0)
				{
					if (size == 1)
					{
						return TSTRING(WAITING_USER_ONLINE);
					}
					else
					{
						tstring buf;
						buf.resize(64);
						buf.resize(snwprintf(&buf[0], buf.size(), CTSTRING(WAITING_USERS_ONLINE), l_online, size));
						return buf;
					}
				}
				else
				{
					switch (size)
					{
						case 0:
							return TSTRING(NO_USERS_TO_DOWNLOAD_FROM);
						case 1:
							return TSTRING(USER_OFFLINE);
						case 2:
							return TSTRING(BOTH_USERS_OFFLINE);
						case 3:
							return TSTRING(ALL_3_USERS_OFFLINE);
						case 4:
							return TSTRING(ALL_4_USERS_OFFLINE);
						default:
							tstring buf;
							buf.resize(64);
							buf.resize(snwprintf(&buf[0], buf.size(), CTSTRING(ALL_USERS_OFFLINE), size));
							return buf;
					};
				}
			}
			else
			{
				if (size == 1)
				{
					return TSTRING(USER_ONLINE);
				}
				else
				{
					tstring buf;
					buf.resize(64);
					buf.resize(snwprintf(&buf[0], buf.size(), CTSTRING(USERS_ONLINE), l_online, size));
					return buf;
				}
			}
		}
		case COLUMN_SEGMENTS:
		{
			const QueueItemPtr& qi = getQueueItem();
			return Util::toStringW(qi->getDownloadsSegmentCount()) + _T('/') + Util::toStringW(qi->getMaxSegments()); // [!] IRainman fix.
		}
		case COLUMN_SIZE:
			return getSize() == -1 ? TSTRING(UNKNOWN) : Util::formatBytesW(getSize());
		case COLUMN_DOWNLOADED:
		{
			// [!] IRainman fix done: https://www.box.net/shared/ns5fr8bk0lrdy5f6z2oo
			return getSize() > 0 ? Util::formatBytesW(getDownloadedBytes()) + _T(" (") + Util::toStringW((double)getDownloadedBytes() * 100.0 / (double)getSize()) + _T("%)") : Util::emptyStringT;
		}
		case COLUMN_PRIORITY:
		{
			tstring priority;
			switch (getPriority())
			{
				case QueueItem::PAUSED:
					priority = TSTRING(PAUSED);
					break;
				case QueueItem::LOWEST:
					priority = TSTRING(LOWEST);
					break;
				case QueueItem::LOW:
					priority = TSTRING(LOW);
					break;
				case QueueItem::NORMAL:
					priority = TSTRING(NORMAL);
					break;
				case QueueItem::HIGH:
					priority = TSTRING(HIGH);
					break;
				case QueueItem::HIGHEST:
					priority = TSTRING(HIGHEST);
					break;
				default:
					dcassert(0);
					break;
			}
			if (getAutoPriority())
			{
				priority += _T(" (") + TSTRING(AUTO) + _T(')');
			}
			return priority;
		}
		case COLUMN_USERS:
		{
			tstring tmp;
			SharedLock l(QueueItem::cs);
			const auto& sources = qi->getSourcesL();
			for (auto j = sources.cbegin(); j != sources.cend(); ++j)
			{
				if (!tmp.empty())
					tmp += _T(", ");
					
				tmp += WinUtil::getNicks(j->getUser(), Util::emptyString);
			}
			return tmp.empty() ? TSTRING(NO_USERS) : tmp;
		}
		case COLUMN_PATH:
		{
			return Text::toT(getPath());
		}
		case COLUMN_LOCAL_PATH: // TODO fix copy-paste
		{
			tstring l_result;
			if (!qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_USER_GET_IP))
			{
				l_result = Text::toT(ShareManager::getInstance()->toRealPath(getTTH()));
				if (l_result.empty())
				{
					const auto l_status_file = CFlylinkDBManager::getInstance()->get_status_file(getTTH());
					if (l_status_file & CFlylinkDBManager::PREVIOUSLY_DOWNLOADED)
						l_result += TSTRING(I_DOWNLOADED_THIS_FILE); //[!]NightOrion(translate)
					if (l_status_file & CFlylinkDBManager::PREVIOUSLY_BEEN_IN_SHARE)
					{
						if (!l_result.empty())
							l_result += _T(" + ");
						l_result +=  TSTRING(THIS_FILE_WAS_IN_MY_SHARE); //[!]NightOrion(translate)
					}
				}
			}
			return l_result;
		}
		case COLUMN_EXACT_SIZE:
		{
			return getSize() == -1 ? TSTRING(UNKNOWN) : Util::formatExactSize(getSize());
		}
		case COLUMN_ERRORS:
		{
			tstring tmp;
			SharedLock l(QueueItem::cs);
			const auto& badSources = qi->getBadSourcesL();
			for (auto j = badSources.cbegin(); j != badSources.cend(); ++j)
			{
				if (!j->isSet(QueueItem::Source::FLAG_REMOVED))
				{
					if (!tmp.empty())
						tmp += _T(", ");
					tmp += WinUtil::getNicks(j->getUser(), Util::emptyString);
					tmp += _T(" (");
					if (j->isSet(QueueItem::Source::FLAG_FILE_NOT_AVAILABLE))
					{
						tmp += TSTRING(FILE_NOT_AVAILABLE);
					}
					else if (j->isSet(QueueItem::Source::FLAG_PASSIVE))
					{
						tmp += TSTRING(PASSIVE_USER);
					}
					else if (j->isSet(QueueItem::Source::FLAG_BAD_TREE))
					{
						tmp += TSTRING(INVALID_TREE);
					}
					else if (j->isSet(QueueItem::Source::FLAG_SLOW_SOURCE))
					{
						tmp += TSTRING(SLOW_USER);
					}
					else if (j->isSet(QueueItem::Source::FLAG_NO_TTHF))
					{
						tmp += TSTRING(SOURCE_TOO_OLD);
					}
					else if (j->isSet(QueueItem::Source::FLAG_NO_NEED_PARTS))
					{
						tmp += TSTRING(NO_NEEDED_PART);
					}
					else if (j->isSet(QueueItem::Source::FLAG_UNTRUSTED))
					{
						tmp += TSTRING(CERTIFICATE_NOT_TRUSTED);
					}
					tmp += _T(')');
				}
			}
			return tmp.empty() ? TSTRING(NO_ERRORS) : tmp;
		}
		case COLUMN_ADDED:
		{
			return Text::toT(Util::formatDigitalClock(getAdded()));
		}
		case COLUMN_TTH:
		{
			return qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_USER_GET_IP) ? Util::emptyStringT : Text::toT(getTTH().toBase32());
		}
		default:
		{
			return Util::emptyStringT;
		}
	}
}

void QueueFrame::on(QueueManagerListener::Added, const QueueItemPtr& aQI)
{
	QueueItemInfo* ii = new QueueItemInfo(aQI);
	
	tasks.add(ADD_ITEM, new QueueItemInfoTask(ii));
}

void QueueFrame::addQueueItem(QueueItemInfo* ii, bool noSort)
{
	if (!ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
	{
		dcassert(ii->getSize() >= 0);
		queueSize += ii->getSize();
	}
	queueItems++;
	m_dirty = true;
	
	const string& dir = ii->getPath();
	
	const auto i = m_directories.find(dir);
	const bool updateDir = (i == m_directories.end());
	m_directories.insert(DirectoryMapPair(dir, ii)); // TODO m_directories.insert(i,DirectoryMapPair(dir, ii));
	if (updateDir)
	{
		addDirectory(dir, ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST));
	}
	if (!showTree || isCurDir(dir))
	{
		if (noSort)
		{
			ctrlQueue.insertItem(ctrlQueue.GetItemCount(), ii, I_IMAGECALLBACK);
		}
		else
		{
			ctrlQueue.insertItem(ii, I_IMAGECALLBACK);
		}
	}
}

QueueFrame::QueueItemInfo* QueueFrame::getItemInfo(const string& target) const
{
	const string path = Util::getFilePath(target);
	DirectoryPairC items = m_directories.equal_range(path);
	for (DirectoryIterC i = items.first; i != items.second; ++i)
	{
#ifdef _DEBUG
		//static int g_count = 0;
		//dcdebug("QueueFrame::getItemInfo  count = %d i->second->getTarget() = %s\n", ++g_count, i->second->getTarget().c_str());
#endif
		if (i->second->getTarget() == target)
		{
			return i->second;
		}
	}
	return nullptr;
}

void QueueFrame::addQueueList()
{
	CLockRedraw<> l_lock_draw(ctrlQueue);
	CLockRedraw<true> l_lock_draw_dir(ctrlDirs);
	{
		// [!] IRainman opt.
		QueueManager::LockFileQueueShared lockedInstance;
		auto& li = lockedInstance.getQueue();
		for (auto j = li.cbegin(); j != li.cend(); ++j)
		{
			const QueueItemPtr& aQI = j->second;
			QueueItemInfo* ii = new QueueItemInfo(aQI);
			addQueueItem(ii, true);
		}
		// [~] IRainman opt.
	}
	ctrlQueue.resort();
}

HTREEITEM QueueFrame::addDirectory(const string& dir, bool isFileList /* = false */, HTREEITEM startAt /* = NULL */)
{
	TVINSERTSTRUCT tvi = {0};
	tvi.hInsertAfter = TVI_SORT;
	tvi.item.mask = TVIF_IMAGE | TVIF_PARAM | TVIF_SELECTEDIMAGE | TVIF_TEXT;
	tvi.item.iImage = tvi.item.iSelectedImage = FileImage::DIR_ICON;
	
	if (isFileList)
	{
		// We assume we haven't added it yet, and that all filelists go to the same
		// directory...
		dcassert(fileLists == NULL);
		tvi.hParent = NULL;
		tvi.item.pszText = _T("File Lists");
		tvi.item.lParam = (LPARAM) new string(dir);
		fileLists = ctrlDirs.InsertItem(&tvi);
		return fileLists;
	}
	
	// More complicated, we have to find the last available tree item and then see...
	string::size_type i = 0;
	string::size_type j;
	
	HTREEITEM next = NULL;
	HTREEITEM parent = NULL;
	
	if (startAt == NULL)
	{
		// First find the correct drive letter or netpath
		dcassert((dir[1] == ':' && dir[2] == '\\') || (dir[0] == '\\' && dir[1] == '\\'));
		
		next = ctrlDirs.GetRootItem();
		
		while (next != NULL)
		{
			if (next != fileLists)
			{
				string* stmp = reinterpret_cast<string*>(ctrlDirs.GetItemData(next));
				if (strnicmp(*stmp, dir, 3) == 0)
					break;
			}
			next = ctrlDirs.GetNextSiblingItem(next);
		}
		
		if (next == NULL)
		{
			// First addition, set commonStart to the dir minus the last part...
			i = dir.rfind('\\', dir.length() - 2);
			if (i != string::npos)
			{
				tstring name = Text::toT(dir.substr(0, i));
				tvi.hParent = NULL;
				tvi.item.pszText = const_cast<TCHAR*>(name.c_str());
				tvi.item.lParam = (LPARAM)new string(dir.substr(0, i + 1));
				next = ctrlDirs.InsertItem(&tvi);
			}
			else
			{
				dcassert((dir.length() == 3 && dir[1] == ':' && dir[2] == '\\') || (dir.length() == 2 && dir[0] == '\\' && dir[1] == '\\'));
				tstring name = Text::toT(dir);
				tvi.hParent = NULL;
				tvi.item.pszText = const_cast<TCHAR*>(name.c_str());
				tvi.item.lParam = (LPARAM)new string(dir);
				next = ctrlDirs.InsertItem(&tvi);
			}
		}
		
		// Ok, next now points to x:\... find how much is common
		
		string* rootStr = (string*)ctrlDirs.GetItemData(next);
		
		i = 0;
		if (rootStr)
			for (;;)
			{
				j = dir.find('\\', i);
				if (j == string::npos)
					break;
				if (strnicmp(dir.c_str() + i, rootStr->c_str() + i, j - i + 1) != 0)
					// 2012-05-11_23-53-01_XA3TUVCW2JPX7NO3B5TJFLW6GOBCKCTGIOZANCY_A2F488A4_crash-stack-r502-beta26-build-9946.dmp
					break;
				i = j + 1;
			}
			
		if (rootStr && i < rootStr->length())
		{
			HTREEITEM oldRoot = next;
			
			// Create a new root
			tstring name = Text::toT(rootStr->substr(0, i - 1));
			tvi.hParent = NULL;
			tvi.item.pszText = const_cast<TCHAR*>(name.c_str());
			tvi.item.lParam = (LPARAM)new string(rootStr->substr(0, i));
			HTREEITEM newRoot = ctrlDirs.InsertItem(&tvi);
			
			parent = addDirectory(*rootStr, false, newRoot);
			
			next = ctrlDirs.GetChildItem(oldRoot);
			while (next != NULL)
			{
				moveNode(next, parent);
				next = ctrlDirs.GetChildItem(oldRoot);
			}
			delete rootStr;
			ctrlDirs.DeleteItem(oldRoot);
			parent = newRoot;
		}
		else
		{
			// Use this root as parent
			parent = next;
			next = ctrlDirs.GetChildItem(parent);
		}
	}
	else
	{
		parent = startAt;
		next = ctrlDirs.GetChildItem(parent);
		i = getDir(parent).length();
		dcassert(strnicmp(getDir(parent), dir, getDir(parent).length()) == 0);
	}
	
	while (i < dir.length())
	{
		while (next != NULL)
		{
			if (next != fileLists)
			{
				const string& n = getDir(next);
				if (!n.empty() && strnicmp(n.c_str() + i, dir.c_str() + i, n.length() - i) == 0) // i = 56 https://www.box.net/shared/3571b0c47c1a8360aec0  n = {npos=4294967295 } https://www.box.net/shared/487b71099375c9313d2a
				{
					// Found a part, we assume it's the best one we can find...
					i = n.length();
					
					parent = next;
					next = ctrlDirs.GetChildItem(next);
					break;
				}
			}
			next = ctrlDirs.GetNextSiblingItem(next);
		}
		
		if (next == NULL)
		{
			// We didn't find it, add...
			j = dir.find('\\', i);
			dcassert(j != string::npos);
			tstring name = Text::toT(dir.substr(i, j - i));
			tvi.hParent = parent;
			tvi.item.pszText = const_cast<TCHAR*>(name.c_str());
			tvi.item.lParam = (LPARAM) new string(dir.substr(0, j + 1));
			
			parent = ctrlDirs.InsertItem(&tvi);
			
			i = j + 1;
		}
	}
	
	return parent;
}

void QueueFrame::removeDirectory(const string& dir, bool isFileList /* = false */)
{

	// First, find the last name available
	string::size_type i = 0;
	
	HTREEITEM next = ctrlDirs.GetRootItem();
	HTREEITEM parent = NULL;
	
	if (isFileList)
	{
		dcassert(fileLists != NULL);
		delete reinterpret_cast<string*>(ctrlDirs.GetItemData(fileLists));
		ctrlDirs.DeleteItem(fileLists);
		fileLists = NULL;
		return;
	}
	else
	{
		while (i < dir.length())
		{
			while (next != NULL)
			{
				if (next != fileLists)
				{
					const string& n = getDir(next);
					if (strnicmp(n.c_str() + i, dir.c_str() + i, n.length() - i) == 0)
					{
						// Match!
						parent = next;
						next = ctrlDirs.GetChildItem(next);
						i = n.length();
						break;
					}
				}
				next = ctrlDirs.GetNextSiblingItem(next);
			}
			if (next == NULL)
				break;
		}
	}
	
	next = parent;
	
	while (ctrlDirs.GetChildItem(next) == NULL && m_directories.find(getDir(next)) == m_directories.end())
	{
		delete reinterpret_cast<string*>(ctrlDirs.GetItemData(next));
		parent = ctrlDirs.GetParentItem(next);
		
		ctrlDirs.DeleteItem(next);
		if (parent == NULL)
			break;
		next = parent;
	}
}

void QueueFrame::removeDirectories(HTREEITEM ht)
{
	HTREEITEM next = ctrlDirs.GetChildItem(ht);
	while (next != NULL)
	{
		removeDirectories(next);
		next = ctrlDirs.GetNextSiblingItem(ht);
	}
	delete reinterpret_cast<string*>(ctrlDirs.GetItemData(ht));
	ctrlDirs.DeleteItem(ht);
}

void QueueFrame::on(QueueManagerListener::Removed, const QueueItemPtr& aQI)
{
	tasks.add(REMOVE_ITEM, new StringTask(aQI->getTarget()));
	
	// we need to call speaker now to properly remove item before other actions
	// [!] SSA - fixed bug with deadlock on opened QueueFrame
	// onSpeaker(0, 0, 0, *reinterpret_cast<BOOL*>(NULL));
}

void QueueFrame::on(QueueManagerListener::Moved, const QueueItemPtr& aQI, const string& oldTarget)
{
	tasks.add(REMOVE_ITEM, new StringTask(oldTarget));
	
	
	// we need to call speaker now to properly remove item before other actions
	// [!] SSA - fixed bug with deadlock on opened QueueFrame (������ � ������ Deadlock'� �� Move'�)
	//onSpeaker(0, 0, 0, *reinterpret_cast<BOOL*>(NULL));
	
	tasks.add(ADD_ITEM, new QueueItemInfoTask(new QueueItemInfo(aQI)));
}

void QueueFrame::on(QueueManagerListener::Tick, const QueueItemList& p_list) noexcept // [+] IRainman opt.
{
	if (!MainFrame::isAppMinimized() && WinUtil::g_tabCtrl->isActive(m_hWnd))
	{
		on(QueueManagerListener::StatusUpdatedList(), p_list);
		tasks.add(UPDATE_STATUSBAR, nullptr); // [!]IRainman optimize QueueFrame
	}
}

LRESULT QueueFrame::onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	TaskQueue::List t;
	
	tasks.get(t);
	//m_spoken = false; [-] IRainman opt.
	
	for (auto ti = t.cbegin(); ti != t.cend(); ++ti)
	{
		switch (ti->first)
		{
			case ADD_ITEM:
			{
				const auto& iit = static_cast<QueueItemInfoTask&>(*ti->second);
				
				dcassert(ctrlQueue.findItem(iit.ii) == -1);
				addQueueItem(iit.ii, false);
				if (!iit.ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP)
				        && BOOLSETTING(BOLD_QUEUE))
				{
					setDirty();
				}
				m_dirty = true;
			}
			break;
			case REMOVE_ITEM:
			{
				const auto& target = static_cast<StringTask&>(*ti->second);
				const QueueItemInfo* ii = getItemInfo(target.getStr());
				if (!ii)
				{
					// Item already delete.
					break;
				}
				
				if (!showTree || isCurDir(ii->getPath()))
				{
					dcassert(ctrlQueue.findItem(ii) != -1);
					ctrlQueue.deleteItem(ii);
				}
				
				if (!ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
				{
					queueSize -= ii->getSize();
					dcassert(queueSize >= 0);
				}
				queueItems--;
				dcassert(queueItems >= 0);
				
				pair<DirectoryIter, DirectoryIter> i = m_directories.equal_range(ii->getPath());
				DirectoryIter j;
				for (j = i.first; j != i.second; ++j)
				{
					if (j->second == ii)
						break;
				}
				dcassert(j != i.second);
				m_directories.erase(j);
				if (m_directories.find(ii->getPath()) == m_directories.end()) // [!] IRainman opt.
				{
					removeDirectory(ii->getPath(), ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST));
					if (isCurDir(ii->getPath()))
						curDir.clear();
				}
				
				if (!ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_USER_GET_IP | QueueItem::FLAG_DCLST_LIST)
				        && BOOLSETTING(BOLD_QUEUE))
				{
					setDirty();
				}
				m_dirty = true;
				
				delete ii;
				
				if (!queueItems)
					updateStatus();// [!]IRainman optimize QueueFrame
			}
			break;
			case UPDATE_ITEM:
			{
				auto &ui = static_cast<UpdateTask&>(*ti->second);
				QueueItemInfo* ii = getItemInfo(ui.target);
				if (!ii)
					break;
					
				if (!showTree || isCurDir(ii->getPath()))
				{
					int pos = ctrlQueue.findItem(ii);
					if (pos != -1)
					{
						ctrlQueue.updateItem(pos, COLUMN_SEGMENTS);
						ctrlQueue.updateItem(pos, COLUMN_PROGRESS);
						ctrlQueue.updateItem(pos, COLUMN_PRIORITY);
						ctrlQueue.updateItem(pos, COLUMN_USERS);
						ctrlQueue.updateItem(pos, COLUMN_ERRORS);
						ctrlQueue.updateItem(pos, COLUMN_STATUS);
						ctrlQueue.updateItem(pos, COLUMN_DOWNLOADED);
					}
				}
			}
			break;
			case UPDATE_STATUS:
			{
				auto& status = static_cast<StringTask&>(*ti->second);
				ctrlStatus.SetText(1, Text::toT(status.getStr()).c_str());
			}
			break;
			case UPDATE_STATUSBAR://[+]IRainman optimize QueueFrame
			{
				updateStatus();
			}
			break;
			default:
				dcassert(0);
				break;
		}
		delete ti->second;
	}
	m_spoken = false; // [+] IRainman opt
	
	return 0;
}

void QueueFrame::removeSelected()
{
	UINT checkState = BOOLSETTING(CONFIRM_DELETE) ? BST_UNCHECKED : BST_CHECKED; // [+] InfinitySky.
	if (checkState == BST_CHECKED || ::MessageBox(m_hWnd, CTSTRING(REALLY_REMOVE), T_APPNAME_WITH_VERSION, CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) == IDYES) // [~] InfinitySky.
	{
		ctrlQueue.forEachSelected(&QueueItemInfo::remove);
		
		// Let's update the setting unchecked box means we bug user again...
		SET_SETTING(CONFIRM_DELETE, checkState != BST_CHECKED); // [+] InfinitySky.
	}
}

void QueueFrame::removeSelectedDir()
{
	if (ctrlDirs.GetSelectedItem())
	{
		UINT checkState = BOOLSETTING(CONFIRM_DELETE) ? BST_UNCHECKED : BST_CHECKED; // [+] InfinitySky.
		if (checkState == BST_CHECKED || ::MessageBox(m_hWnd, CTSTRING(REALLY_REMOVE), T_APPNAME_WITH_VERSION, CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) == IDYES) // [~] InfinitySky.
		{
			removeDir(ctrlDirs.GetSelectedItem());
			// [+] NightOrion bugfix deleting folder from queue
			for (auto i = m_tmp_target_to_delete.cbegin(); i != m_tmp_target_to_delete.cend(); ++i)
				QueueManager::getInstance()->remove(*i);
				
			m_tmp_target_to_delete.clear();
			// [+] NightOrion
			
			// Let's update the setting unchecked box means we bug user again...
			SET_SETTING(CONFIRM_DELETE, checkState != BST_CHECKED); // [+] InfinitySky.
		}
	}
}
void QueueFrame::moveSelected()
{
	tstring name;
	if (showTree)
	{
		name = Text::toT(curDir);
	}
	
	if (WinUtil::browseDirectory(name, m_hWnd))
	{
		// [!] IRainman fix http://code.google.com/p/flylinkdc/issues/detail?id=82
		vector<const QueueItemInfo*> l_movingItems;
		int i = -1;
		while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			const QueueItemInfo* ii = ctrlQueue.getItemData(i);
			l_movingItems.push_back(ii);
		}
		const string l_toDir = Text::fromT(name);
		for (auto i = l_movingItems.cbegin(); i != l_movingItems.end(); ++i)
		{
			const QueueItemInfo* ii = *i;
			const auto& l_target = ii->getTarget();
			QueueManager::getInstance()->move(l_target, l_toDir + Util::getFileName(l_target));
		}
		// [~] IRainman fix.
	}
}

void QueueFrame::moveTempArray()
{
	for (auto i = m_move_temp_array.cbegin(); i != m_move_temp_array.cend(); ++i)
	{
		QueueManager::getInstance()->move((*i).first->getTarget(), (*i).second + Util::getFileName((*i).first->getTarget()));
	}
	m_move_temp_array.clear();
}

void QueueFrame::moveSelectedDir()
{
	if (ctrlDirs.GetSelectedItem() == NULL)
		return;
		
	dcassert(!curDir.empty());
	tstring name = Text::toT(curDir);
	
	if (WinUtil::browseDirectory(name, m_hWnd))
	{
		m_move_temp_array.clear();
		moveDir(ctrlDirs.GetSelectedItem(), Text::fromT(name) + Util::getLastDir(getDir(ctrlDirs.GetSelectedItem())) + PATH_SEPARATOR);
		moveTempArray();
	}
}

void QueueFrame::moveDir(HTREEITEM ht, const string& target)
{
	HTREEITEM next = ctrlDirs.GetChildItem(ht);
	while (next != NULL)
	{
		// must add path separator since getLastDir only give us the name
		moveDir(next, target + Util::getLastDir(getDir(next)) + PATH_SEPARATOR);
		next = ctrlDirs.GetNextSiblingItem(next);
	}
	
	string* s = (string*)ctrlDirs.GetItemData(ht);
	
	DirectoryPairC p = m_directories.equal_range(*s);
	
	for (auto i = p.first; i != p.second; ++i)
	{
		m_move_temp_array.push_back(TempMovePair(i->second, target));
	}
}

void QueueFrame::renameSelected()
{
	// Single file, get the full filename and move...
	const QueueItemInfo* ii = getSelectedQueueItem();
	const tstring target = Text::toT(ii->getTarget());
	const tstring filename = Util::getFileName(target);
	
	LineDlg dlg;
	dlg.title = TSTRING(RENAME);
	dlg.description = TSTRING(FILENAME);
	dlg.line = filename;
	if (dlg.DoModal(m_hWnd) == IDOK)
		QueueManager::getInstance()->move(ii->getTarget(), Text::fromT(target.substr(0, target.length() - filename.length()) + dlg.line));
}

void QueueFrame::renameSelectedDir()
{
	if (ctrlDirs.GetSelectedItem() == NULL)
		return;
		
	dcassert(!curDir.empty());
	const string lname = Util::getLastDir(getDir(ctrlDirs.GetSelectedItem()));
	LineDlg dlg;
	dlg.description = TSTRING(DIRECTORY);
	dlg.title = TSTRING(RENAME);
	dlg.line = Text::toT(lname);
	if (dlg.DoModal(m_hWnd) == IDOK)
	{
		const string name = curDir.substr(0, curDir.length() - (lname.length() + 2)) + PATH_SEPARATOR + Text::fromT(dlg.line) + PATH_SEPARATOR;
		m_move_temp_array.clear();
		moveDir(ctrlDirs.GetSelectedItem(), name);
		moveTempArray();
	}
}

LRESULT QueueFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{

	OMenu priorityMenu;
	priorityMenu.CreatePopupMenu();
#ifdef OLD_MENU_HEADER //[~]JhaoDa
	priorityMenu.InsertSeparatorFirst(TSTRING(PRIORITY));
#endif
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_PAUSED, CTSTRING(PAUSED));
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_LOWEST, CTSTRING(LOWEST));
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_LOW, CTSTRING(LOW));
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_NORMAL, CTSTRING(NORMAL));
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_HIGH, CTSTRING(HIGH));
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_HIGHEST, CTSTRING(HIGHEST));
	priorityMenu.AppendMenu(MF_STRING, IDC_AUTOPRIORITY, CTSTRING(AUTO));
	
	if (reinterpret_cast<HWND>(wParam) == ctrlQueue && ctrlQueue.GetSelectedCount() > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlQueue, pt);
		}
		
		OMenu segmentsMenu;
		segmentsMenu.CreatePopupMenu();
#ifdef OLD_MENU_HEADER //[~]JhaoDa
		segmentsMenu.InsertSeparatorFirst(TSTRING(MAX_SEGMENTS_NUMBER));
#endif
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTONE, (_T("1 ") + TSTRING(SEGMENT)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTTWO, (_T("2 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTTHREE, (_T("3 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTFOUR, (_T("4 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTFIVE, (_T("5 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTSIX, (_T("6 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTSEVEN, (_T("7 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTEIGHT, (_T("8 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTNINE, (_T("9 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTTEN, (_T("10 ") + TSTRING(SEGMENTS)).c_str());
		
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTFIFTY, (_T("50 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTHUNDRED, (_T("100 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTHUNDRED_FIFTY, (_T("150 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTTWO_HUNDRED, (_T("200 ") + TSTRING(SEGMENTS)).c_str());
		
		if (ctrlQueue.GetSelectedCount() > 0)
		{
			usingDirMenu = false;
			CMenuItemInfo mi;
			
			while (browseMenu.GetMenuItemCount() > 0)
			{
				browseMenu.RemoveMenu(0, MF_BYPOSITION);
			}
			while (removeMenu.GetMenuItemCount() > 2)
			{
				removeMenu.RemoveMenu(2, MF_BYPOSITION);
			}
			while (removeAllMenu.GetMenuItemCount() > 0)
			{
				removeAllMenu.RemoveMenu(0, MF_BYPOSITION);
			}
			while (pmMenu.GetMenuItemCount() > 0)
			{
				pmMenu.RemoveMenu(0, MF_BYPOSITION);
			}
			while (readdMenu.GetMenuItemCount() > 2)
			{
				readdMenu.RemoveMenu(2, MF_BYPOSITION);
			}
			clearPreviewMenu();
			
			if (ctrlQueue.GetSelectedCount() == 1)
			{
				OMenu copyMenu;
				copyMenu.CreatePopupMenu();
#ifdef OLD_MENU_HEADER //[~]JhaoDa
				copyMenu.InsertSeparatorFirst(TSTRING(COPY));
#endif
				copyMenu.AppendMenu(MF_STRING, IDC_COPY_LINK, CTSTRING(COPY_MAGNET_LINK));
				for (int i = 0; i < COLUMN_LAST; ++i)
					copyMenu.AppendMenu(MF_STRING, IDC_COPY + i, CTSTRING_I(columnNames[i]));
					
				OMenu singleMenu;
				singleMenu.CreatePopupMenu();
#ifdef OLD_MENU_HEADER //[~]JhaoDa
				singleMenu.InsertSeparatorFirst(TSTRING(FILE));
#endif
				singleMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES));
				appendPreviewItems(singleMenu);
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)segmentsMenu, CTSTRING(MAX_SEGMENTS_NUMBER));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)priorityMenu, CTSTRING(SET_PRIORITY));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)browseMenu, CTSTRING(GET_FILE_LIST));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)pmMenu, CTSTRING(SEND_PRIVATE_MESSAGE));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)readdMenu, CTSTRING(READD_SOURCE));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyMenu, CTSTRING(COPY));
				singleMenu.AppendMenu(MF_SEPARATOR);
				singleMenu.AppendMenu(MF_STRING, IDC_MOVE, CTSTRING(MOVE));
				singleMenu.AppendMenu(MF_STRING, IDC_RENAME, CTSTRING(RENAME));
				singleMenu.AppendMenu(MF_SEPARATOR);
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)removeMenu, CTSTRING(REMOVE_SOURCE));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)removeAllMenu, CTSTRING(REMOVE_FROM_ALL));
				singleMenu.AppendMenu(MF_STRING, IDC_REMOVE_OFFLINE, CTSTRING(REMOVE_OFFLINE));
				singleMenu.AppendMenu(MF_SEPARATOR);
				singleMenu.AppendMenu(MF_STRING, IDC_RECHECK, CTSTRING(RECHECK_FILE));
				singleMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
				singleMenu.SetMenuDefaultItem(IDC_SEARCH_ALTERNATES); // !SMT!-UI
				
				const QueueItemInfo* ii = getSelectedQueueItem();
				// [-] if (!ii)
				// [-]  return 0;
				{
					// [!] IRainman fix.
					// [-] QueueManager::LockInstance l_instance;
					// [-] const QueueItem* qi = QueueManager::getInstance()->fileQueue.find(ii->getTarget());
					const QueueItemPtr& qi = ii->getQueueItem(); // [+]
					// [~] IRainman fix.
					segmentsMenu.CheckMenuItem(IDC_SEGMENTONE - 1 + qi->getMaxSegments(), MF_CHECKED);
				}
				if (!ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
				{
					setupPreviewMenu(ii->getTarget());
					activatePreviewItems(singleMenu);
				}
				else
				{
					singleMenu.EnableMenuItem((UINT)(HMENU)segmentsMenu, MFS_DISABLED);
				}
				
				menuItems = 0;
				int pmItems = 0;
				//
				{
					SharedLock l(QueueItem::cs);
					const auto& sources = ii->getQueueItem()->getSourcesL(); // ������ ����� ������� - http://code.google.com/p/flylinkdc/issues/detail?id=1270
					// ���� ��������� ����� ���������
					for (auto i = sources.cbegin(); i != sources.cend(); ++i)
					{
						tstring nick = WinUtil::escapeMenu(WinUtil::getNicks(i->getUser(), Util::emptyString));
						// add hub hint to menu
						const auto& user = i->getUser(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'i->getUser()' expression repeatedly. queueframe.cpp 1067
						const auto& hubs = ClientManager::getInstance()->getHubNames(user->getCID(), Util::emptyString);
						if (!hubs.empty())
							nick += _T(" (") + Text::toT(hubs[0]) + _T(")");
							
						mi.fMask = MIIM_ID | MIIM_TYPE | MIIM_DATA;
						mi.fType = MFT_STRING;
						mi.dwTypeData = (LPTSTR)nick.c_str();
						mi.dwItemData = (ULONG_PTR) & (*i); // http://code.google.com/p/flylinkdc/issues/detail?id=1270
						mi.wID = IDC_BROWSELIST + menuItems;
						browseMenu.InsertMenuItem(menuItems, TRUE, &mi);
						mi.wID = IDC_REMOVE_SOURCE + 1 + menuItems; // "All" is before sources
						removeMenu.InsertMenuItem(menuItems + 2, TRUE, &mi); // "All" and separator come first
						mi.wID = IDC_REMOVE_SOURCES + menuItems;
						removeAllMenu.InsertMenuItem(menuItems, TRUE, &mi);
						if (user->isOnline())
						{
							mi.wID = IDC_PM + menuItems;
							pmMenu.InsertMenuItem(menuItems, TRUE, &mi);
							pmItems++;
						}
						menuItems++;
					}
					readdItems = 0;
					const auto& badSources = ii->getQueueItem()->getBadSourcesL(); // ������ ����� ������� - http://code.google.com/p/flylinkdc/issues/detail?id=1270
					// ���� ��������� ����� ���������
					for (auto i = badSources.cbegin(); i != badSources.cend(); ++i)
					{
						const auto& user = i->getUser();
						tstring nick = WinUtil::getNicks(user, Util::emptyString);
						if (i->isSet(QueueItem::Source::FLAG_FILE_NOT_AVAILABLE))
						{
							nick += _T(" (") + TSTRING(FILE_NOT_AVAILABLE) + _T(")");
						}
						else if (i->isSet(QueueItem::Source::FLAG_PASSIVE))
						{
							nick += _T(" (") + TSTRING(PASSIVE_USER) + _T(")");
						}
						else if (i->isSet(QueueItem::Source::FLAG_BAD_TREE))
						{
							nick += _T(" (") + TSTRING(INVALID_TREE) + _T(")");
						}
						else if (i->isSet(QueueItem::Source::FLAG_NO_NEED_PARTS))
						{
							nick += _T(" (") + TSTRING(NO_NEEDED_PART) + _T(")");
						}
						else if (i->isSet(QueueItem::Source::FLAG_NO_TTHF))
						{
							nick += _T(" (") + TSTRING(SOURCE_TOO_OLD) + _T(")");
						}
						else if (i->isSet(QueueItem::Source::FLAG_SLOW_SOURCE))
						{
							nick += _T(" (") + TSTRING(SLOW_USER) + _T(")");
						}
						else if (i->isSet(QueueItem::Source::FLAG_UNTRUSTED))
						{
							nick += _T(" (") + TSTRING(CERTIFICATE_NOT_TRUSTED) + _T(")");
						}
						// add hub hint to menu
						const auto& hubs = ClientManager::getInstance()->getHubNames(user->getCID(), Util::emptyString);
						if (!hubs.empty())
							nick += _T(" (") + Text::toT(hubs[0]) + _T(")");
							
						mi.fMask = MIIM_ID | MIIM_TYPE | MIIM_DATA;
						mi.fType = MFT_STRING;
						mi.dwTypeData = (LPTSTR)nick.c_str();
						mi.dwItemData = (ULONG_PTR) & (*i); // http://code.google.com/p/flylinkdc/issues/detail?id=1270
						mi.wID = IDC_READD + 1 + readdItems;  // "All" is before sources
						readdMenu.InsertMenuItem(readdItems + 2, TRUE, &mi);  // "All" and separator come first
						readdItems++;
					}
				}
				
				if (menuItems == 0)
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)browseMenu, MFS_DISABLED);
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)removeMenu, MFS_DISABLED);
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)removeAllMenu, MFS_DISABLED);
				}
				else
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)browseMenu, MFS_ENABLED);
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)removeMenu, MFS_ENABLED);
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)removeAllMenu, MFS_ENABLED);
				}
				
				if (pmItems == 0)
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)pmMenu, MFS_DISABLED);
				}
				else
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)pmMenu, MFS_ENABLED);
				}
				
				if (readdItems == 0)
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)readdMenu, MFS_DISABLED);
				}
				else
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)readdMenu, MFS_ENABLED);
				}
				
				priorityMenu.CheckMenuItem(ii->getPriority(), MF_BYPOSITION | MF_CHECKED);
				if (ii->getAutoPriority())
					priorityMenu.CheckMenuItem(7, MF_BYPOSITION | MF_CHECKED);
					
#ifdef OLD_MENU_HEADER //[~]JhaoDa
				browseMenu.InsertSeparatorFirst(TSTRING(GET_FILE_LIST));
				removeMenu.InsertSeparatorFirst(TSTRING(REMOVE_SOURCE));
				removeAllMenu.InsertSeparatorFirst(TSTRING(REMOVE_FROM_ALL));
				pmMenu.InsertSeparatorFirst(TSTRING(SEND_PRIVATE_MESSAGE));
				readdMenu.InsertSeparatorFirst(TSTRING(READD_SOURCE));
#endif
				
				singleMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
				
#ifdef OLD_MENU_HEADER //[~]JhaoDa
				browseMenu.RemoveFirstItem();
				removeMenu.RemoveFirstItem();
				removeAllMenu.RemoveFirstItem();
				pmMenu.RemoveFirstItem();
				readdMenu.RemoveFirstItem();
#endif
			}
			else
			{
				OMenu multiMenu;
				multiMenu.CreatePopupMenu();
#ifdef OLD_MENU_HEADER //[~]JhaoDa
				multiMenu.InsertSeparatorFirst(TSTRING(FILES));
#endif
				multiMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES)); // !SMT!-UI
				multiMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)segmentsMenu, CTSTRING(MAX_SEGMENTS_NUMBER));
				multiMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)priorityMenu, CTSTRING(SET_PRIORITY));
				multiMenu.AppendMenu(MF_STRING, IDC_MOVE, CTSTRING(MOVE));
				multiMenu.AppendMenu(MF_SEPARATOR);
				multiMenu.AppendMenu(MF_STRING, IDC_REMOVE_OFFLINE, CTSTRING(REMOVE_OFFLINE));
				multiMenu.AppendMenu(MF_STRING, IDC_RECHECK, CTSTRING(RECHECK_FILE));
				multiMenu.AppendMenu(MF_SEPARATOR);
				multiMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
				multiMenu.SetMenuDefaultItem(IDC_SEARCH_ALTERNATES); // !SMT!-UI
				multiMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			}
			
			return TRUE;
		}
	}
	else if (reinterpret_cast<HWND>(wParam) == ctrlDirs && ctrlDirs.GetSelectedItem() != NULL)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlDirs, pt);
		}
		else
		{
			// Strange, windows doesn't change the selection on right-click... (!)
			UINT a = 0;
			ctrlDirs.ScreenToClient(&pt);
			HTREEITEM ht = ctrlDirs.HitTest(pt, &a);
			if (ht != NULL && ht != ctrlDirs.GetSelectedItem())
				ctrlDirs.SelectItem(ht);
			ctrlDirs.ClientToScreen(&pt);
		}
		usingDirMenu = true;
		
		OMenu dirMenu;
		dirMenu.CreatePopupMenu();
#ifdef OLD_MENU_HEADER //[~]JhaoDa
		dirMenu.InsertSeparatorFirst(TSTRING(FOLDER));
#endif
		dirMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)priorityMenu, CTSTRING(SET_PRIORITY));
		dirMenu.AppendMenu(MF_STRING, IDC_MOVE, CTSTRING(MOVE));
		dirMenu.AppendMenu(MF_STRING, IDC_RENAME, CTSTRING(RENAME));
		dirMenu.AppendMenu(MF_SEPARATOR);
		dirMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
		dirMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		
		return TRUE;
	}
	
	bHandled = FALSE;
	return FALSE;
}

LRESULT QueueFrame::onRecheck(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	StringList tmp;
	int i = -1;
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
		tmp.push_back(ii->getTarget());
	}
	for (auto i = begin(tmp); i != end(tmp); ++i)
	{
		QueueManager::getInstance()->recheck(*i);
	}
	return 0;
}

LRESULT QueueFrame::onSearchAlternates(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// !SMT!-UI (multiple search)
	int i = -1;
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
		WinUtil::searchHash(ctrlQueue.getItemData(i)->getTTH());
	return 0;
}

LRESULT QueueFrame::onCopyMagnet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		int i = ctrlQueue.GetNextItem(-1, LVNI_SELECTED);
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
		WinUtil::copyMagnet(ii->getTTH(), Util::getFileName(ii->getTarget()), ii->getSize());
	}
	return 0;
}

LRESULT QueueFrame::onBrowseList(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		CMenuItemInfo mi;
		mi.fMask = MIIM_DATA;
		browseMenu.GetMenuItemInfo(wID, FALSE, &mi);
		OMenuItem* omi = (OMenuItem*)mi.dwItemData;
		if (omi)
		{
			QueueItem::Source* s   = (QueueItem::Source*)omi->m_data;
			dcassert(getSelectedQueueItem() && QueueManager::getInstance()->isSourceValid(getSelectedQueueItem()->getQueueItem(), s));
			// �������� ���� http://code.google.com/p/flylinkdc/issues/detail?id=1270
			try
			{
				const auto& hubs = ClientManager::getInstance()->getHubNames(s->getUser()->getCID(), Util::emptyString);
				QueueManager::getInstance()->addList(HintedUser(s->getUser(), !hubs.empty() ? hubs[0] : Util::emptyString), QueueItem::FLAG_CLIENT_VIEW);
			}
			catch (const Exception&)
			{
			}
		}
	}
	return 0;
}

LRESULT QueueFrame::onReadd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		int i = ctrlQueue.GetNextItem(-1, LVNI_SELECTED);
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
		
		CMenuItemInfo mi;
		mi.fMask = MIIM_DATA;
		
		readdMenu.GetMenuItemInfo(wID, FALSE, &mi);
		if (wID == IDC_READD)
		{
			try
			{
				QueueManager::getInstance()->readdAll(ii->getQueueItem());
			}
			catch (const QueueException& e)
			{
				ctrlStatus.SetText(1, Text::toT(e.getError()).c_str());
			}
		}
		else
		{
			OMenuItem* omi = (OMenuItem*)mi.dwItemData;
			if (omi)
			{
				QueueItem::Source* s = (QueueItem::Source*)omi->m_data;
				try
				{
					QueueManager::getInstance()->readd(ii->getTarget(), s->getUser());
				}
				catch (const QueueException& e)
				{
					ctrlStatus.SetText(1, Text::toT(e.getError()).c_str());
				}
			}
		}
	}
	return 0;
}

LRESULT QueueFrame::onRemoveSource(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		int i = ctrlQueue.GetNextItem(-1, LVNI_SELECTED);
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
		if (wID == IDC_REMOVE_SOURCE)
		{
			UniqueLock l(QueueItem::cs);
			const auto& sources = ii->getQueueItem()->getSourcesL();
			for (auto si = sources.cbegin(); si != sources.cend(); ++si)
			{
				// TODO - ������ ��� ���� ����������� UniqueLock lqi(QueueItem::cs);
				// ��������� ��� ��������� �� ����
				QueueManager::getInstance()->removeSource(ii->getTarget(), si->getUser(), QueueItem::Source::FLAG_REMOVED);
			}
		}
		else
		{
			CMenuItemInfo mi;
			mi.fMask = MIIM_DATA;
			removeMenu.GetMenuItemInfo(wID, FALSE, &mi);
			OMenuItem* omi = (OMenuItem*)mi.dwItemData;
			if (omi)
			{
				QueueItem::Source* s = (QueueItem::Source*)omi->m_data;
				QueueManager::getInstance()->removeSource(ii->getTarget(), s->getUser(), QueueItem::Source::FLAG_REMOVED);
			}
		}
	}
	return 0;
}

LRESULT QueueFrame::onRemoveSources(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CMenuItemInfo mi;
	mi.fMask = MIIM_DATA;
	removeAllMenu.GetMenuItemInfo(wID, FALSE, &mi);
	OMenuItem* omi = (OMenuItem*)mi.dwItemData;
	if (omi)
	{
		if (QueueItem::Source* s = (QueueItem::Source*)omi->m_data)
		{
			QueueManager::getInstance()->removeSource(s->getUser(), QueueItem::Source::FLAG_REMOVED);
		}
	}
	return 0;
}

LRESULT QueueFrame::onPM(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		CMenuItemInfo mi;
		mi.fMask = MIIM_DATA;
		
		pmMenu.GetMenuItemInfo(wID, FALSE, &mi);
		OMenuItem* omi = (OMenuItem*)mi.dwItemData;
		if (omi)
		{
			if (QueueItem::Source* s = (QueueItem::Source*)omi->m_data)
			{
				// [!] IRainman: Open the window of PM with an empty address if the user NMDC,
				// as soon as it appears on the hub of the network, the window immediately PM knows about it, and update the Old.
				// If the user ADC, as soon as he appears on any of the ADC hubs at once a personal window to know.
				const auto& hubs = ClientManager::getInstance()->getHubs(s->getUser()->getCID(), Util::emptyString);
				PrivateFrame::openWindow(nullptr, HintedUser(s->getUser(), !hubs.empty() ? hubs[0] : Util::emptyString));
			}
		}
	}
	return 0;
}

LRESULT QueueFrame::onAutoPriority(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{

	if (usingDirMenu)
	{
		setAutoPriority(ctrlDirs.GetSelectedItem(), true);
	}
	else
	{
		int i = -1;
		while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			QueueManager::getInstance()->setAutoPriority(ctrlQueue.getItemData(i)->getTarget(), !ctrlQueue.getItemData(i)->getAutoPriority());
		}
	}
	return 0;
}

LRESULT QueueFrame::onSegments(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		QueueItemInfo* ii = ctrlQueue.getItemData(i);
		{
			// [!] IRainman fix.
			// [-] QueueManager::LockInstance l_instance;
			// [-] QueueItem* qi = QueueManager::getInstance()->fileQueue.find(ii->getTarget());
			const QueueItemPtr& qi = ii->getQueueItem();
			// [~] IRainman fix.
			qi->setMaxSegments(max((uint8_t)1, (uint8_t)(wID - IDC_SEGMENTONE + 1))); // !BUGMASTER!-S
		}
		ctrlQueue.updateItem(ctrlQueue.findItem(ii), COLUMN_SEGMENTS);
	}
	
	return 0;
}

LRESULT QueueFrame::onPriority(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	QueueItem::Priority p;
	
	switch (wID)
	{
		case IDC_PRIORITY_PAUSED:
			p = QueueItem::PAUSED;
			break;
		case IDC_PRIORITY_LOWEST:
			p = QueueItem::LOWEST;
			break;
		case IDC_PRIORITY_LOW:
			p = QueueItem::LOW;
			break;
		case IDC_PRIORITY_NORMAL:
			p = QueueItem::NORMAL;
			break;
		case IDC_PRIORITY_HIGH:
			p = QueueItem::HIGH;
			break;
		case IDC_PRIORITY_HIGHEST:
			p = QueueItem::HIGHEST;
			break;
		default:
			p = QueueItem::DEFAULT;
			break;
	}
	
	if (usingDirMenu)
	{
		setPriority(ctrlDirs.GetSelectedItem(), p);
	}
	else
	{
		int i = -1;
		while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			// TODO - ������� ��������� � ��������� - ������� ������
			const auto& l_target = ctrlQueue.getItemData(i)->getTarget();
			QueueManager::getInstance()->setAutoPriority(l_target, false);
			QueueManager::getInstance()->setPriority(l_target, p);
		}
	}
	
	return 0;
}

void QueueFrame::removeDir(HTREEITEM ht)
{
	if (ht == NULL)
		return;
	HTREEITEM child = ctrlDirs.GetChildItem(ht);
	while (child != NULL)
	{
		removeDir(child);
		child = ctrlDirs.GetNextSiblingItem(child);
	}
	const string& name = getDir(ht);
	DirectoryPairC dp = m_directories.equal_range(name);
//	StringList l_tmp_target; // [-] NightOrion bugfix deleting folder from queue
	for (auto i = dp.first; i != dp.second; ++i)
		m_tmp_target_to_delete.push_back(i->second->getTarget());
//	for (auto i = l_tmp_target.cbegin(); i != l_tmp_target.cend(); ++i)
//		QueueManager::getInstance()->remove(*i); //
}

/*
 * @param inc True = increase, False = decrease
 */
void QueueFrame::changePriority(bool inc)
{
	int i = -1;
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		QueueItem::Priority p = ctrlQueue.getItemData(i)->getPriority();
		
		if ((inc && p == QueueItem::HIGHEST) || (!inc && p == QueueItem::PAUSED))
		{
			// Trying to go higher than HIGHEST or lower than PAUSED
			// so do nothing
			continue;
		}
		
		switch (p)
		{
			case QueueItem::HIGHEST:
				p = QueueItem::HIGH;
				break;
			case QueueItem::HIGH:
				p = inc ? QueueItem::HIGHEST : QueueItem::NORMAL;
				break;
			case QueueItem::NORMAL:
				p = inc ? QueueItem::HIGH    : QueueItem::LOW;
				break;
			case QueueItem::LOW:
				p = inc ? QueueItem::NORMAL  : QueueItem::LOWEST;
				break;
			case QueueItem::LOWEST:
				p = inc ? QueueItem::LOW     : QueueItem::PAUSED;
				break;
			case QueueItem::PAUSED:
				p = QueueItem::LOWEST;
				break;
			default:
				dcassert(false);
				break;
		}
		// TODO - ������� ��������� � ��������� - ������� ������
		const auto& l_target = ctrlQueue.getItemData(i)->getTarget();
		QueueManager::getInstance()->setAutoPriority(l_target, false);
		QueueManager::getInstance()->setPriority(l_target, p);
	}
}

void QueueFrame::setPriority(HTREEITEM ht, const QueueItem::Priority& p)
{
	if (ht == NULL)
		return;
	HTREEITEM child = ctrlDirs.GetChildItem(ht);
	while (child)
	{
		setPriority(child, p);
		child = ctrlDirs.GetNextSiblingItem(child);
	}
	const string& name = getDir(ht);
	DirectoryPairC dp = m_directories.equal_range(name);
	for (auto i = dp.first; i != dp.second; ++i)
	{
		// TODO - ������� ��������� � ��������� - ������� ������
		// + ������ ��� ����
		QueueManager::getInstance()->setAutoPriority(i->second->getTarget(), false);
		QueueManager::getInstance()->setPriority(i->second->getTarget(), p);
	}
}

void QueueFrame::setAutoPriority(HTREEITEM ht, const bool& ap)
{
	if (ht == NULL)
		return;
	HTREEITEM child = ctrlDirs.GetChildItem(ht);
	while (child)
	{
		setAutoPriority(child, ap);
		child = ctrlDirs.GetNextSiblingItem(child);
	}
	const string& name = getDir(ht);
	DirectoryPairC dp = m_directories.equal_range(name);
	for (auto i = dp.first; i != dp.second; ++i)
	{
		QueueManager::getInstance()->setAutoPriority(i->second->getTarget(), ap);
	}
}

void QueueFrame::updateStatus()
{
	if (ctrlStatus.IsWindow())
	{
		int64_t total = 0;
		int cnt = ctrlQueue.GetSelectedCount();
		if (cnt == 0)
		{
			cnt = ctrlQueue.GetItemCount();
			if (showTree)
			{
				int i = -1;
				while ((i = ctrlQueue.GetNextItem(i, LVNI_ALL)) != -1)
				{
					const QueueItemInfo* ii = ctrlQueue.getItemData(i);
					if (ii)
					{
						const int64_t l_size = ii->getSize(); // [5] https://www.box.net/shared/3b3f3d75561df0d66772
						if (l_size > 0)
							total += l_size;
						// 2012-05-03_22-00-59_XU5MNUJZMTMQRSM747KAIDWAUX5TB4W467OAELI_2DCDA2E5_crash-stack-r502-beta24-build-9900.dmp
						// 2012-05-11_23-57-17_U4TZSIYHFC32XEVTFRRT6A5T746QRL3VBVRZ6OY_FDCFBE73_crash-stack-r502-beta26-x64-build-9946.dmp
						// 2012-05-11_23-53-01_IZIM6B7B5FPUY3NUHXVN2J3JVYYHLXC5PNMVRTA_833FF3DF_crash-stack-r502-beta26-build-9946.dmp
					}
				}
			}
			else
			{
				total = queueSize;
			}
		}
		else
		{
			int i = -1;
			while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				const QueueItemInfo* ii = ctrlQueue.getItemData(i);
				total += (ii->getSize() > 0) ? ii->getSize() : 0; // [1] https://www.box.net/shared/98f8cd9c265dca2fe830
			}
			
		}
		
		tstring tmp1 = TSTRING(ITEMS) + _T(": ") + Util::toStringW(cnt);
		tstring tmp2 = TSTRING(SIZE) + _T(": ") + Util::formatBytesW(total);
		bool u = false;
		
		int w = WinUtil::getTextWidth(tmp1, ctrlStatus.m_hWnd);
		if (statusSizes[1] < w)
		{
			statusSizes[1] = w;
			u = true;
		}
		ctrlStatus.SetText(2, tmp1.c_str());
		w = WinUtil::getTextWidth(tmp2, ctrlStatus.m_hWnd);
		if (statusSizes[2] < w)
		{
			statusSizes[2] = w;
			u = true;
		}
		ctrlStatus.SetText(3, tmp2.c_str());
		
		if (m_dirty)
		{
			tmp1 = TSTRING(FILES) + _T(": ") + Util::toStringW(queueItems);
			tmp2 = TSTRING(SIZE) + _T(": ") + Util::formatBytesW(queueSize);
			
			w = WinUtil::getTextWidth(tmp2, ctrlStatus.m_hWnd);
			if (statusSizes[3] < w)
			{
				statusSizes[3] = w;
				u = true;
			}
			ctrlStatus.SetText(4, tmp1.c_str());
			
			w = WinUtil::getTextWidth(tmp2, ctrlStatus.m_hWnd);
			if (statusSizes[4] < w)
			{
				statusSizes[4] = w;
				u = true;
			}
			ctrlStatus.SetText(5, tmp2.c_str());
			
			m_dirty = false;
		}
		
		if (u)
			UpdateLayout(TRUE);
	}
}

void QueueFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	
	if (ctrlStatus.IsWindow())
	{
		CRect sr;
		int w[6];
		ctrlStatus.GetClientRect(sr);
		w[5] = sr.right - 16;
#define setw(x) w[x] = max(w[x+1] - statusSizes[x], 0)
		setw(4);
		setw(3);
		setw(2);
		setw(1);
		
		w[0] = 16;
		
		ctrlStatus.SetParts(6, w);
		
		ctrlStatus.GetRect(0, sr);
		ctrlShowTree.MoveWindow(sr);
	}
	
	if (showTree)
	{
		if (GetSinglePaneMode() != SPLIT_PANE_NONE)
		{
			SetSinglePaneMode(SPLIT_PANE_NONE);
			updateQueue();
		}
	}
	else
	{
		if (GetSinglePaneMode() != SPLIT_PANE_RIGHT)
		{
			SetSinglePaneMode(SPLIT_PANE_RIGHT);
			updateQueue();
		}
	}
	
	CRect rc = rect;
	SetSplitterRect(rc);
}

LRESULT QueueFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!m_closed)
	{
		m_closed = true;
		safe_destroy_timer();
		SettingsManager::getInstance()->removeListener(this);
		QueueManager::getInstance()->removeListener(this);
		
		WinUtil::setButtonPressed(IDC_QUEUE, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		HTREEITEM ht = ctrlDirs.GetRootItem();
		while (ht != NULL)
		{
			clearTree(ht);
			ht = ctrlDirs.GetNextSiblingItem(ht);
		}
		
		SET_SETTING(QUEUEFRAME_SHOW_TREE, ctrlShowTree.GetCheck() == BST_CHECKED);
		for (auto i = m_directories.cbegin(); i != m_directories.cend(); ++i)
		{
			delete i->second;
		}
		m_directories.clear();
		ctrlQueue.DeleteAllItems();
		
		ctrlQueue.saveHeaderOrder(SettingsManager::QUEUEFRAME_ORDER,
		                          SettingsManager::QUEUEFRAME_WIDTHS, SettingsManager::QUEUEFRAME_VISIBLE);
		                          
		SET_SETTING(QUEUE_COLUMNS_SORT, ctrlQueue.getSortColumn());
		SET_SETTING(QUEUE_COLUMNS_SORT_ASC, ctrlQueue.isAscending());
		SET_SETTING(QUEUEFRAME_SPLIT, m_nProportionalPos);
		bHandled = FALSE;
		return 0;
	}
}

LRESULT QueueFrame::onItemChanged(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/)
{
	updateQueue();
	return 0;
}

void QueueFrame::onTab()
{
	if (showTree)
	{
		HWND focus = ::GetFocus();
		if (focus == ctrlDirs.m_hWnd)
		{
			ctrlQueue.SetFocus();
		}
		else if (focus == ctrlQueue.m_hWnd)
		{
			ctrlDirs.SetFocus();
		}
	}
}

void QueueFrame::updateQueue()
{
	CWaitCursor l_cursor_wait;
	ctrlQueue.DeleteAllItems();
	pair<DirectoryIter, DirectoryIter> i;
	if (showTree)
	{
		i = m_directories.equal_range(getSelectedDir());
	}
	else
	{
		i.first = m_directories.begin();
		i.second = m_directories.end();
	}
	
	CLockRedraw<> l_lock_draw(ctrlQueue);
	auto l_count = ctrlQueue.GetItemCount();
	for (auto j = i.first; j != i.second; ++j)
	{
		QueueItemInfo* ii = j->second;
		ctrlQueue.insertItem(l_count++, ii, I_IMAGECALLBACK);
	}
	ctrlQueue.resort();
	curDir = getSelectedDir();
}

void QueueFrame::clearTree(HTREEITEM item)
{
	HTREEITEM next = ctrlDirs.GetChildItem(item);
	while (next != NULL)
	{
		clearTree(next);
		next = ctrlDirs.GetNextSiblingItem(next);
	}
	delete reinterpret_cast<string*>(ctrlDirs.GetItemData(item));
}

// Put it here to avoid a copy for each recursion...
void QueueFrame::moveNode(HTREEITEM item, HTREEITEM parent)
{
	static TCHAR tmpBuf[1024];
	tmpBuf[0] = 0;
	TVINSERTSTRUCT tvis = {0};
	tvis.itemex.hItem = item;
	tvis.itemex.mask = TVIF_CHILDREN | TVIF_HANDLE | TVIF_IMAGE | TVIF_INTEGRAL | TVIF_PARAM |
	                   TVIF_SELECTEDIMAGE | TVIF_STATE | TVIF_TEXT;
	tvis.itemex.pszText = tmpBuf;
	tvis.itemex.cchTextMax = _countof(tmpBuf);//[!] IRainman use _countof(Array) ;)
	ctrlDirs.GetItem((TVITEM*)&tvis.itemex);
	tvis.hInsertAfter = TVI_SORT;
	tvis.hParent = parent;
	tvis.item.mask &= ~TVIF_HANDLE; // !SMT!-F
	HTREEITEM ht = ctrlDirs.InsertItem(&tvis);
	HTREEITEM next = ctrlDirs.GetChildItem(item);
	while (next != NULL)
	{
		moveNode(next, ht);
		next = ctrlDirs.GetChildItem(item);
	}
	ctrlDirs.DeleteItem(item);
}

LRESULT QueueFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)pnmh;
	
	QueueItemInfo *qii = (QueueItemInfo*)cd->nmcd.lItemlParam;
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
		case CDDS_ITEMPREPAINT:
		{
			if (// [!] IRainman fix: needs for test! Please report to me if crashing here.
			    // [-] qii && qii->getQueueItem() &&
			    !qii->getQueueItem()->getBadSourcesL().empty()) // [!] IRainman fix done [7] https://www.box.net/shared/f516cb76187b328e4bf5
			{
				cd->clrText = SETTING(ERROR_COLOR);
				Colors::alternationBkColor(cd); // [+] IRainman
				return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
			}
			Colors::alternationBkColor(cd); // [+] IRainman
			return CDRF_NOTIFYSUBITEMDRAW;
		}
		
		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
			if (ctrlQueue.findColumn(cd->iSubItem) == COLUMN_PROGRESS)
			{
				if (!BOOLSETTING(SHOW_PROGRESS_BARS))
				{
					bHandled = FALSE;
					return 0;
				}
				// [-]if (!qii) return CDRF_DODEFAULT; [-] IRainman fix.
				if (qii->getSize() == -1) return CDRF_DODEFAULT;
				
				// draw something nice...
				CRect rc;
				ctrlQueue.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
				CBarShader statusBar(rc.Height(), rc.Width(), SETTING(PROGRESS_BACK_COLOR), qii->getSize());
				
				/* [-] IRainman fix.
				    vector<Segment> v;
				
				    // running chunks
				    QueueManager::getInstance()->getChunksVisualisation(qii->getQueueItem(), 0, v);
				    for (auto i = v.cbegin(); i < v.cend(); ++i)
				    {
				        statusBar.FillRange((*i).getStart(), (*i).getEnd(), SETTING(COLOR_RUNNING));
				    }
				
				    // downloaded bytes
				    QueueManager::getInstance()->getChunksVisualisation(qii->getQueueItem(), 1, v);
				    for (auto i = v.cbegin(); i < v.cend(); ++i)
				    {
				        statusBar.FillRange((*i).getStart(), (*i).getEnd(), SETTING(COLOR_DOWNLOADED));
				    }
				
				    // done chunks
				    QueueManager::getInstance()->getChunksVisualisation(qii->getQueueItem(), 2, v);
				    for (auto i = v.cbegin(); i < v.cend(); ++i)
				    {
				        statusBar.FillRange((*i).getStart(), (*i).getEnd(), SETTING(COLOR_DOWNLOADED));
				    }
				 [-] IRainman fix. */
				// [+] IRainman fix.
				vector<pair<Segment, Segment>> l_runnigChunksAndDownloadBytes;
				vector<Segment> l_doneChunks;
				
				QueueManager::getChunksVisualisation(qii->getQueueItem(), l_runnigChunksAndDownloadBytes, l_doneChunks);
				for (auto i = l_runnigChunksAndDownloadBytes.cbegin(); i < l_runnigChunksAndDownloadBytes.cend(); ++i)
				{
					statusBar.FillRange((*i).first.getStart(), (*i).first.getEnd(), SETTING(COLOR_RUNNING));
					statusBar.FillRange((*i).second.getStart(), (*i).second.getEnd(), SETTING(COLOR_DOWNLOADED));
				}
				for (auto i = l_doneChunks.cbegin(); i < l_doneChunks.cend(); ++i)
				{
					statusBar.FillRange((*i).getStart(), (*i).getEnd(), SETTING(COLOR_DOWNLOADED));
				}
				// [~] IRainman fix.
				
				CDC cdc;
				cdc.CreateCompatibleDC(cd->nmcd.hdc);
				HBITMAP pOldBmp = cdc.SelectBitmap(CreateCompatibleBitmap(cd->nmcd.hdc,  rc.Width(),  rc.Height()));
				
				statusBar.Draw(cdc, 0, 0, SETTING(PROGRESS_3DDEPTH));
				BitBlt(cd->nmcd.hdc, rc.left, rc.top, rc.Width(), rc.Height(), cdc.m_hDC, 0, 0, SRCCOPY);
				DeleteObject(cdc.SelectBitmap(pOldBmp));
				
				return CDRF_SKIPDEFAULT;
			}
		}
		default:
			return CDRF_DODEFAULT;
	}
}

LRESULT QueueFrame::onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring data;
	int i = -1, columnId = wID - IDC_COPY; // !SMT!-UI: copy several rows
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		QueueItemInfo* ii = ctrlQueue.getItemData(i);
		tstring sdata;
		// !SMT!-UI
		if (wID == IDC_COPY_LINK)
			sdata = Text::toT(Util::getMagnet(ii->getTTH(), Util::getFileName(ii->getTarget()), ii->getSize()));
		else if (wID == IDC_COPY_WMLINK)
			sdata = Text::toT(Util::getWebMagnet(ii->getTTH(), Util::getFileName(ii->getTarget()), ii->getSize()));
		else
			sdata = ii->getText(columnId);
			
		if (data.empty())
			data = sdata;
		else
			data = data + L"\r\n" + sdata;
	}
	WinUtil::setClipboard(data);
	return 0;
}

LRESULT QueueFrame::onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		const QueueItemInfo* i = getSelectedQueueItem();
		// [!] IRainman fix.
		// [-] QueueManager::LockInstance l_instance;
		// [-] QueueItem* qi = QueueManager::getInstance()->fileQueue.find(i->getTarget());
		// [-] if (qi)
		const QueueItemPtr& qi = i->getQueueItem();
		// [~] IRainman fix.
		startMediaPreview(wID, qi);
	}
	return 0;
}

LRESULT QueueFrame::onRemoveOffline(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
		
		UniqueLock l(QueueItem::cs);
		const auto& sources = ii->getQueueItem()->getSourcesL();
		for (auto i =  sources.cbegin(); i != sources.cend(); ++i)
		{
			if (!i->getUser()->isOnline())
			{
				// TODO - ������ ��� ���� ����������� UniqueLock lqi(QueueItem::cs);
				// ��������� ��� ��������� �� ����
				QueueManager::getInstance()->removeSource(ii->getTarget(), i->getUser(), QueueItem::Source::FLAG_REMOVED);
			}
		}
	}
	return 0;
}

void QueueFrame::on(SettingsManagerListener::Save, SimpleXML& /*xml*/) noexcept
{
	bool refresh = false;
	if (ctrlQueue.GetBkColor() != Colors::bgColor)
	{
		ctrlQueue.SetBkColor(Colors::bgColor);
		ctrlQueue.SetTextBkColor(Colors::bgColor);
		ctrlQueue.setFlickerFree(Colors::bgBrush);
		ctrlDirs.SetBkColor(Colors::bgColor);
		refresh = true;
	}
	if (ctrlQueue.GetTextColor() != Colors::textColor)
	{
		ctrlQueue.SetTextColor(Colors::textColor);
		ctrlDirs.SetTextColor(Colors::textColor);
		refresh = true;
	}
	if (refresh == true)
	{
		RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	}
}

void QueueFrame::onRechecked(const string& target, const string& message)
{
	string buf;
	buf.resize(STRING(INTEGRITY_CHECK).length() + message.length() + target.length() + 16);
	sprintf(&buf[0], CSTRING(INTEGRITY_CHECK), message.c_str(), target.c_str());
	
	tasks.add(UPDATE_STATUS, new StringTask(buf));
}

void QueueFrame::on(QueueManagerListener::RecheckStarted, const string& target) noexcept
{
	onRechecked(target, STRING(STARTED));
}

void QueueFrame::on(QueueManagerListener::RecheckNoFile, const string& target) noexcept
{
	onRechecked(target, STRING(UNFINISHED_FILE_NOT_FOUND));
}

void QueueFrame::on(QueueManagerListener::RecheckFileTooSmall, const string& target) noexcept
{
	onRechecked(target, STRING(UNFINISHED_FILE_TOO_SMALL));
}

void QueueFrame::on(QueueManagerListener::RecheckDownloadsRunning, const string& target) noexcept
{
	onRechecked(target, STRING(DOWNLOADS_RUNNING));
}

void QueueFrame::on(QueueManagerListener::RecheckNoTree, const string& target) noexcept
{
	onRechecked(target, STRING(NO_FULL_TREE));
}

void QueueFrame::on(QueueManagerListener::RecheckAlreadyFinished, const string& target) noexcept
{
	onRechecked(target, STRING(FILE_ALREADY_FINISHED));
}

void QueueFrame::on(QueueManagerListener::RecheckDone, const string& target) noexcept
{
	onRechecked(target, STRING(DONE));
}

/**
 * @file
 * $Id: QueueFrame.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
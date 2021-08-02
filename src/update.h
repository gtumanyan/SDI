/*
This file is part of Snappy Driver Installer.

Snappy Driver Installer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License or (at your option) any later version.

Snappy Driver Installer is distributed in the hope that it will be useful
but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
Snappy Driver Installer.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UPDATE_H
#define UPDATE_H

// Declarations
class Updater_t;
class Canvas;

// Global variables
extern Updater_t *Updater;

struct type_item {
    wchar_t ItemName[BUFSIZ];
    __int64 SizeMB;
    __int64 Percent;
    int VersionNew;
    int VersionCurrent;
    wchar_t ForThisPC[BUFSIZ];
    int DefaultSort;
    };

// Updater
class Updater_t
{
public:
	int numfiles=0;
    static bool SeedMode;
    static int torrentport,downlimit,uplimit,connections,activetorrent;
    static const std::wstring torrent_url;
    static const std::wstring torrent2_url;
    static const std::wstring torrent_save_path;
    static const std::wstring torrent2_save_path;
public:
    virtual ~Updater_t(){}

    virtual void ShowProgress(wchar_t *buf)=0;
    virtual void ShowPopup(Canvas &canvas)=0;

    virtual void checkUpdates()=0;
    virtual void resumeDownloading()=0;
    virtual void pause()=0;

    virtual bool isTorrentReady()=0;
    virtual bool isPaused()=0;
    virtual bool isUpdateCompleted()=0;
    virtual bool isSeedingDrivers()=0;

    virtual int  Populate(int flags)=0;
    virtual void SetFilePriority(const wchar_t *name,int pri)=0;
    virtual void SetLimits()=0;
    virtual void OpenDialog()=0;
    virtual void DownloadAll()=0;
    virtual void DownloadNetwork()=0;
    virtual void DownloadIndexes()=0;
    virtual void StartSeedingDrivers()=0;
    virtual void StopSeedingDrivers()=0;

    virtual int scriptInitUpdates(int torrentport)=0;
    virtual int scriptDownloadApp()=0;
    virtual int scriptDownloadIndexes()=0;
    virtual int scriptDownloadDrivers(std::wstring mode)=0;
    virtual int scriptDownloadEverything()=0;
    virtual int scriptDoDownload()=0;
    virtual int scriptInstall()=0;
};
Updater_t *CreateUpdater();

#endif

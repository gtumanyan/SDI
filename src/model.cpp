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

#include "com_header.h"
#include "common.h"
#include "logging.h"
#include "system.h"
#include "settings.h"
#include "indexing.h"
#include "matcher.h"
#include "manager.h"
#include "update.h"

#include <windows.h>

// Depend on Win32API
#include "enum.h"
#include "main.h"
#include "model.h"

extern Event *deviceupdate_event;
extern volatile int deviceupdate_exitflag;
extern int bundle_display;
extern int bundle_shadow;

//{ Bundle
unsigned int __stdcall Bundle::thread_scandevices(void *arg)
{
    State *state=static_cast<State *>(arg);

    if((invaidate_set&INVALIDATE_DEVICES)==0)return 0;

    if(Settings.statemode==STATEMODE_REAL)state->scanDevices();
    if(Settings.statemode==STATEMODE_EMUL)state->load(Settings.state_file);

    return 0;
}

unsigned int __stdcall Bundle::thread_loadindexes(void *arg)
{
    Collection *collection=static_cast<Collection *>(arg);

    if(invaidate_set&INVALIDATE_INDEXES)collection->updatedir();
    return 0;
}

unsigned int __stdcall Bundle::thread_getsysinfo(void *arg)
{
    State *state=static_cast<State *>(arg);

    if(Settings.statemode==STATEMODE_REAL&&invaidate_set&INVALIDATE_SYSINFO)
        state->getsysinfo_slow();
    return 0;
}

Bundle::Bundle()
{
    matcher=CreateMatcher();
    bundle_init();
}

Bundle::~Bundle()
{
    delete matcher;
}

unsigned int __stdcall Bundle::thread_loadall(void *arg)
{
    Bundle *bundle=static_cast<Bundle *>(arg);

    InitializeCriticalSection(&sync);
    CRITICAL_SECTION_ACTIVE=true;
    while(1)
    {
        // Wait for an update request
        deviceupdate_event->wait();
        if(deviceupdate_exitflag)break;
        bundle[bundle_shadow].bundle_init();
            /*static long long prmem;
            Log.print_con("Total mem:%ld KB(%ld)\n",nvwa::total_mem_alloc/1024,nvwa::total_mem_alloc-prmem);
            prmem=nvwa::total_mem_alloc;*/

        // Update bundle
        Log.print_con("*** START *** %d,%d [%d]\n",bundle_display,bundle_shadow,invaidate_set);
        bundle[bundle_shadow].bundle_prep();
        bundle[bundle_shadow].bundle_load(&bundle[bundle_display]);

        // Check if the state has been udated during scanning
        int cancel_update=0;
        if(!(Settings.flags&FLAG_NOGUI))
        if(deviceupdate_event->isRaised())cancel_update=1;

        if(cancel_update)
        {
            Log.print_con("*** CANCEL ***\n\n");
            deviceupdate_event->raise();
        }
        else
        {
            Log.print_con("*** FINISH primary ***\n\n");
            invaidate_set&=~(INVALIDATE_DEVICES|INVALIDATE_INDEXES|INVALIDATE_SYSINFO);

            if((Settings.flags&FLAG_NOGUI)&&(Settings.flags&FLAG_AUTOINSTALL)==0)
            {
                // NOGUI mode
                manager_g->matcher=bundle[bundle_shadow].matcher;
                manager_g->populate();
                manager_g->filter(Settings.filters);
                bundle[bundle_shadow].bundle_lowpriority();
                break;
            }
            else // GUI mode
            {
                if(MainWindow.hMain)SendMessage(MainWindow.hMain,WM_BUNDLEREADY,(WPARAM)&bundle[bundle_shadow],(LPARAM)&bundle[bundle_display]);
            }

            // Save indexes, write info, etc
            Log.print_con("{2Sync\n");
            if(CRITICAL_SECTION_ACTIVE)EnterCriticalSection(&sync);

            bundle[bundle_shadow].bundle_lowpriority();
            Log.print_con("*** FINISH secondary ***\n\n");

            // Swap display and shadow bundle
            bundle_display^=1;
            bundle_shadow^=1;
            Log.print_con("}2Sync\n");
            bundle[bundle_shadow].bundle_init();
            PostMessage(MainWindow.hMain,WM_INDEXESSAVED,0,0);
            if(CRITICAL_SECTION_ACTIVE)LeaveCriticalSection(&sync);
        }
    }

    CRITICAL_SECTION_ACTIVE=false;
    DeleteCriticalSection(&sync);
    return 0;
}

void Bundle::bundle_init()
{
    state.init();
    collection.init(Settings.drp_dir,Settings.index_dir,Settings.output_dir);
    matcher->init(&state,&collection);
}

void Bundle::bundle_prep()
{
    Log.print_debug("Bundle::bundle_prep\n");
    state.getsysinfo_fast();
    Log.print_debug("Bundle::bundle_prep::complete\n");
}
void Bundle::bundle_load(Bundle *pbundle)
{
    Log.print_debug("Bundle::bundle_load\n");
    ThreadAbs *thandle0=CreateThread();
    ThreadAbs *thandle1=CreateThread();
    ThreadAbs *thandle2=CreateThread();

    Timers.start(time_test);

    // Copy data from shadow if it's not updated
    if((invaidate_set&INVALIDATE_DEVICES)==0)
    {
        state=pbundle->state;
        Timers.reset(time_devicescan);
        if(invaidate_set&INVALIDATE_SYSINFO)state.getsysinfo_fast();
    }
    if((invaidate_set&INVALIDATE_SYSINFO)==0)state.getsysinfo_slow(&pbundle->state);
    if((invaidate_set&INVALIDATE_INDEXES)==0){collection=pbundle->collection;Timers.reset(time_indexes);}

    Log.print_debug("Bundle::bundle_load::thread_scandevices\n");
    thandle0->start(&thread_scandevices,&state);
    Log.print_debug("Bundle::bundle_load::thread_loadindexes\n");
    thandle1->start(&thread_loadindexes,&collection);
    Log.print_debug("Bundle::bundle_load::thread_getsysinfo\n");
    thandle2->start(&thread_getsysinfo,&state);
    Log.print_debug("Bundle::bundle_load::thandle0->join\n");
    thandle0->join();
    Log.print_debug("Bundle::bundle_load::thandle1->join\n");
    thandle1->join();
    Log.print_debug("Bundle::bundle_load::thandle2->join\n");
    thandle2->join();
    delete thandle0;
    delete thandle1;
    delete thandle2;

    /*if((invaidate_set&INVALIDATE_DEVICES)==0)
    {
        state=pbundle->state;time_devicescan=0;}*/

    state.isnotebook_a();
    state.genmarker();
    matcher->getState()->textas.shrink();
    matcher->populate();
    Timers.stop(time_test);

    Log.print_debug("Bundle::bundle_load::complete\n");
}

void Bundle::bundle_lowpriority()
{
    Timers.stoponce(time_startup,time_total);
    //Timers.print();

    MainWindow.redrawmainwnd();

    collection.printstats();
    state.print();
    matcher->print();
    manager_g->print_hr();
    if(wcslen(Settings.device_list_filename)>0)
        matcher->write_device_list(Settings.device_list_filename);

    if(Settings.flags&FLAG_SCRIPTMODE)
    {
        collection.save();
        return;
    }

    #ifdef USE_TORRENT
    if(Settings.flags&FLAG_CHECKUPDATES&&!Timers.get(time_chkupdate))
        Updater->checkUpdates();
    #endif

    collection.save();
    Log.gen_timestamp();
    WStringShort filename;
    filename.sprintf(L"%s\\%sstate.snp",Settings.log_dir,Log.getTimestamp());
    state.save(filename.Get());

    if(Settings.flags&COLLECTION_PRINT_INDEX)
    {
        Log.print_con("Saving humanreadable indexes...\n");
        collection.print_index_hr();
        Settings.flags&=~COLLECTION_PRINT_INDEX;
        Log.print_con("DONE\n");
    }
}
//}

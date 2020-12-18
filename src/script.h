/*
This file is part of Snappy Driver Installer Origin.

Snappy Driver Installer Origin is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License or (at your option) any later version.

Snappy Driver Installer Origin is distributed in the hope that it will be useful
but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
Snappy Driver Installer Origin.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SCRIPT_H
#define SCRIPT_H

#include <vector>
#include "manager.h"

extern Manager *manager_g;

class Script
{
    std::vector<std::wstring>ScriptText;
    public:
        Script();
        virtual ~Script();
        bool loadscript();
        bool runscript();
        static bool cmdArgIsPresent();
    protected:

    private:
        wchar_t *ltrim(wchar_t *s);
        void selectNone();
        void selectAll();
        int updatesInitialised();
        void doParameters(std::wstring &cmd);
        std::vector<std::wstring> parameters;
        void RunLatest(std::wstring args);
};

#endif // SCRIPT_H

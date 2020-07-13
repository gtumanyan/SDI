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

#ifndef MODEL_H
#define MODEL_H

class State;
class Collection;
class Matcher;

//Bundle
class Bundle
{
    Bundle(const Bundle&)=delete;
	Bundle &operator = (const Bundle&) = delete;

    State state;
    Collection collection;
    Matcher *matcher;

private:
    static unsigned int __stdcall thread_scandevices(void *arg);
    static unsigned int __stdcall thread_loadindexes(void *arg);
    static unsigned int __stdcall thread_getsysinfo(void *arg);

public:
    Bundle();
    ~Bundle();

    Matcher *getMatcher(){return matcher;}

    static unsigned int __stdcall thread_loadall(void *arg);

    void bundle_init();
    void bundle_prep();
    void bundle_load(Bundle *pbundle);
    void bundle_lowpriority();
};

#endif

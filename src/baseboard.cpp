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

#include "com_header.h"
#include "common.h"
#include "logging.h"

#include <comdef.h>         // for _bstr_t
#include <Wbemidl.h>        // for IWbemLocator

// Depend on Win32API
#include "enum.h"

void ShowProgressInTaskbar(HWND hwnd,bool show,long long complited,long long total);

int initsec=0;
int State::getbaseboard(WStringShort &manuf1,WStringShort &model1,WStringShort &product1,WStringShort &cs_manuf1,WStringShort &cs_model1,int *type)
{
    *type=0;

    HRESULT hres;
    hres=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(hres))
    {
        Log.print_err("FAILED to initialize COM library. Error code = 0x%lX\n",hres);
        return 0;
    }

    if(!initsec)
    {
        hres=CoInitializeSecurity(nullptr,-1,nullptr,nullptr,RPC_C_AUTHN_LEVEL_DEFAULT,
                                  RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE,nullptr);
        /*if(FAILED(hres))
        {
            Log.print_err("FAILED to initialize security. Error code = 0x%lX\n",hres);
            //CoUninitialize();
            return 0;
        }*/
    }

    IWbemLocator *pLoc=nullptr;
    hres=CoCreateInstance(CLSID_WbemLocator,nullptr,CLSCTX_INPROC_SERVER,IID_IWbemLocator,(LPVOID *)&pLoc);
    if(FAILED(hres))
    {
        Log.print_err("FAILED to create IWbemLocator object. Error code = 0x%lX\n",hres);
        //CoUninitialize();
        return 0;
    }

    IWbemServices *pSvc=nullptr;
    hres=pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"),nullptr,nullptr,nullptr,0,nullptr,nullptr,&pSvc);
    if(FAILED(hres))
    {
        Log.print_err("FAILED to connect to root\\cimv2. Error code = 0x%lX\n",hres);
        pLoc->Release();
        //CoUninitialize();
        return 0;
    }

    //printf("Connected to ROOT\\CIMV2 WMI namespace\n");

    hres=CoSetProxyBlanket(pSvc,RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,nullptr,
                           RPC_C_AUTHN_LEVEL_CALL,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE);
    if(FAILED(hres))
    {
        Log.print_err("FAILED to set proxy blanket. Error code = 0x%lX\n",hres);
        pSvc->Release();
        pLoc->Release();
        //CoUninitialize();
        return 0;
    }

    IEnumWbemClassObject *pEnumerator=nullptr;
    hres=pSvc->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT * FROM Win32_BaseBoard"),
        WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&pEnumerator);
    if(FAILED(hres))
    {
        Log.print_err("FAILED to query for Win32_BaseBoard. Error code = 0x%lX\n",hres);
        pSvc->Release();
        pLoc->Release();
        //CoUninitialize();
        return 0;
    }
    else
    {
        IWbemClassObject *pclsObj;
        ULONG uReturn=0;

        while(pEnumerator)
        {
            pEnumerator->Next(WBEM_INFINITE,1,&pclsObj,&uReturn);
            if(0==uReturn)break;

            VARIANT vtProp1,vtProp2,vtProp3;

            vtProp1.bstrVal=nullptr;
            pclsObj->Get(L"Manufacturer",0,&vtProp1,nullptr,nullptr);
            if(vtProp1.bstrVal)manuf1.strcpy(vtProp1.bstrVal);

            vtProp2.bstrVal=nullptr;
            hres=pclsObj->Get(L"Model",0,&vtProp2,nullptr,nullptr);
            if(vtProp2.bstrVal)model1.strcpy(vtProp2.bstrVal);

            vtProp3.bstrVal=nullptr;
            pclsObj->Get(L"Product",0,&vtProp3,nullptr,nullptr);
            if(vtProp3.bstrVal)product1.strcpy(vtProp3.bstrVal);
        }
    }

    hres=pSvc->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT * FROM Win32_ComputerSystem"),
        WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&pEnumerator);
    if(FAILED(hres))
    {
        Log.print_err("FAILED to query for Win32_ComputerSystem. Error code = 0x%lX\n",hres);
        pSvc->Release();
        pLoc->Release();
        //CoUninitialize();
        return 0;
    }
    else
    {
        IWbemClassObject *pclsObj;
        ULONG uReturn=0;

        while(pEnumerator)
        {
            pEnumerator->Next(WBEM_INFINITE,1,&pclsObj,&uReturn);
            if(0==uReturn)break;

            VARIANT vtProp1,vtProp2;

            vtProp1.bstrVal=nullptr;
            pclsObj->Get(L"Manufacturer",0,&vtProp1,nullptr,nullptr);
            if(vtProp1.bstrVal)cs_manuf1.strcpy(vtProp1.bstrVal);

            vtProp2.bstrVal=nullptr;
            pclsObj->Get(L"Model",0,&vtProp2,nullptr,nullptr);
            if(vtProp2.bstrVal)cs_model1.strcpy(vtProp2.bstrVal);
        }
    }

    hres=pSvc->ExecQuery(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT * FROM Win32_SystemEnclosure"),
        WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&pEnumerator);
    if(FAILED(hres))
    {
        Log.print_err("FAILED to query for Win32_SystemEnclosure. Error code = 0x%lX\n",hres);
        pSvc->Release();
        pLoc->Release();
        //CoUninitialize();
        return 0;
    }
    else
    {
        IWbemClassObject *pclsObj;
        ULONG uReturn=0;

        while(pEnumerator)
        {
            pEnumerator->Next(WBEM_INFINITE,1,&pclsObj,&uReturn);
            if(0==uReturn)break;

            VARIANT vtProp;
            hres=pclsObj->Get(L"ChassisTypes",0,&vtProp,nullptr,nullptr);// Uint16
            if(!FAILED(hres))
            {
                if((vtProp.vt==VT_NULL)||(vtProp.vt==VT_EMPTY))
                    *type=0;
                else
                    if((vtProp.vt&VT_ARRAY))
                    {
                        LONG lLower,lUpper;
                        UINT32 Element=0;
                        SAFEARRAY *pSafeArray=vtProp.parray;
                        SafeArrayGetLBound(pSafeArray,1,&lLower);
                        SafeArrayGetUBound(pSafeArray,1,&lUpper);

                        for(LONG i=lLower;i<=lUpper;i++)
                        {
                            hres=SafeArrayGetElement(pSafeArray,&i,&Element);
                            *type=Element;
                        }
                        SafeArrayDestroy(pSafeArray);
                    }
            }
        }
    }

    initsec=1;

    pSvc->Release();
    pLoc->Release();
    //CoUninitialize();

    return 1;
}

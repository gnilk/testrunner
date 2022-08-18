// trun_utests.cpp : Defines the exported functions for the DLL.
//

#include "pch.h"
#include "framework.h"
#include "trun_utests.h"


// This is an example of an exported variable
TRUNUTESTS_API int ntrunutests=0;

// This is an example of an exported function.
TRUNUTESTS_API int fntrunutests(void)
{
    return 0;
}

// This is the constructor of a class that has been exported.
Ctrunutests::Ctrunutests()
{
    return;
}

// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the TRUNUTESTS_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// TRUNUTESTS_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef TRUNUTESTS_EXPORTS
#define TRUNUTESTS_API __declspec(dllexport)
#else
#define TRUNUTESTS_API __declspec(dllimport)
#endif

// This class is exported from the dll
class TRUNUTESTS_API Ctrunutests {
public:
	Ctrunutests(void);
	// TODO: add your methods here.
};

extern TRUNUTESTS_API int ntrunutests;

TRUNUTESTS_API int fntrunutests(void);

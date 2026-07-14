//
// Created by gnilk on 29.10.24.
//

#ifndef TESTRUNNER_STD_BACKPORT_H
#define TESTRUNNER_STD_BACKPORT_H

// MSVC only updates __cplusplus to the real standard version when /Zc:__cplusplus is passed
// (otherwise it's permanently 199711L regardless of /std:); _MSVC_LANG always reports correctly.
#if (defined(_MSVC_LANG) ? _MSVC_LANG : __cplusplus) > 201703L
    // c++20 and above, no need...
#else
namespace std {
  template<typename _Tp, typename _Alloc, typename _Predicate>
    inline typename vector<_Tp, _Alloc>::size_type
    erase_if(vector<_Tp, _Alloc>& __cont, _Predicate __pred)
    {
      const auto __osz = __cont.size();
      __cont.erase(std::remove_if(__cont.begin(), __cont.end(), __pred),
		   __cont.end());
      return __osz - __cont.size();
    }
}
#endif


#endif //TESTRUNNER_STD_BACKPORT_H

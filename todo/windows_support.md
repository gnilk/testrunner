## Feature: Add back windows support for V3

Windows support was dropped after V1. Several reasons:
- V2 and onwards use weak symbols to detect version, this is not supported on windows
- Forking, this can be mitigated but wasn't
- Probably quite a few other things which I never really cared about (threading model and few other spring to mind)

Primary target is the `trun` application. As I don't have experience with `tcov` on Windows I am unsure how this could work.
It is heavily dependent on CLang but would probably need a complete new backend implementation for the Windows Debugging API's.




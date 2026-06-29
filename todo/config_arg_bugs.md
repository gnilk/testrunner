## Bugs: Command-line argument parsing (Config::FromArguments) — ✅ RESOLVED (dev)

> Both fixed on `dev`. Failing-first tests in
> `src/testrunner/tests/test_config.cpp` (module `config`): `twithoutm`,
> `dumponly`, `deepbind`. Suite unchanged otherwise (fork == sequential).
> #1: deleted the duplicate `-t`→module-filter block. #2: deep-binding now keys
> off `-D` (`!IsPresent("-D")`), matching the help text and the old switch parser.



Both bugs are in the live, non-embedded parser
(`src/testrunner/config.cpp`, the `ArgParser`-based body, lines 119-214). The
older switch-based `old_Config_FromArguments` is dead (`TRUN_EMBEDDED`, and the
call is commented out) — don't confuse the two while fixing.

### 1. `-t` without `-m` silently runs nothing

`config.cpp:163-169`
```cpp
if (argParser.IsPresent("-t")) {              // 163
    ...
    ParseModuleFilters(testModules.c_str());  // 167  <-- duplicated, should not exist
}
if (argParser.IsPresent("-t")) {              // 171
    ...
    ParseTestCaseFilters(testCases.c_str());  // 175  correct
}
if (argParser.IsPresent("-m")) {              // 178
    ...
    ParseModuleFilters(testModules.c_str());  // 182  correct
}
```
Block 163-169 wrongly feeds `-t` into the *module* filter.
`ParseModuleFilters` *replaces* the list (`config.cpp:90`,
`Config::Instance().modules = modules;`), overwriting the default `"-"`
(match-all). So `./trun -t split lib.so` sets `modules = ["split"]`; unless a
module is literally named `split`, nothing executes.

It is masked in the documented `-m X -t Y` form only because the `-m` block runs
*later* (178-184) and overwrites `modules` back. The bare `-t <case>` form is
broken.

- Delete the duplicate block 163-169 (`-t` should only populate test-case filters)

### 2. `-d` controls two unrelated settings

`config.cpp:139` & `141`
```cpp
Config::Instance().dumpConfig          =  argParser.IsPresent("-d");
Config::Instance().linuxUseDeepBinding = !argParser.IsPresent("-d");
```
Passing `-d` to dump the config *also* disables `RTLD_DEEPBIND` on Linux — i.e.
it quietly changes dlopen symbol resolution for the loaded test libraries, which
can change test behaviour. Two features collided on the same letter.

- Give one of them its own flag (e.g. keep `-d` for dump-config, move deep-binding
  to a dedicated `--no-deep-bind` / similar), and decouple the two assignments

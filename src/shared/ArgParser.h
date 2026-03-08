//
// Created by gnilk on 28.11.25.
//

#ifndef GNILK_ARGPARSER_H
#define GNILK_ARGPARSER_H


#include <span>
#include <string>
#include <string_view>
#include <ranges>
#include <charconv>
#include <vector>

//
// simple decent modern argument parser
//
// See tests/test_argparser.cpp for examples or the example/ex1_app.cpp
//
// Features:
//  - Flags present/non-present (on/off toggles)
//  - Count the presence of an option across all arguments (like; verbose level)
//  - Arguments carrying single value
//  - Catch all at the end
//
// Unsupported features:
//  - advanced 'catch end'
//     like:  ./app -i <input> <out1> <out2> <out3>
//     'input' would be part of the catch-end array
//     possible to check with 'IsLastArgument' but I would not recommend using this if you such advanced cmd-lines..
//
// Use from main like:
//      argParser = ArgParse(argc, argv);
//      auto value = argParser.TryParse({}, "-i", "--input_file");
//      bool doSomething = argParser.IsPresent("-f", "--flag");
//      auto value2 = argParser.TryParse(42, "-n", "--number");
//
class ArgParser {
protected:
    enum class kParseResult {
        Ok,
        OkNotPresent,
        ErrMissingArg,
        ErrArgTypeError,
    };
public:
    ArgParser() = delete;
    ArgParser(size_t argc, const char **argv) : args{argv, argc} {
    }
    virtual ~ArgParser() = default;

    // Parse flags (true/false) based on presence of an option...  expecting no arguments...
    [[nodiscard]]
    bool IsPresent(const std::string &shortParamName, const std::string &longParamName = {}) const {
        auto cbValue = [](int idxParam) { return kParseResult::Ok; };
        auto res = TryParseInternal(false, cbValue, shortParamName, longParamName);
        if (res != kParseResult::Ok) {
            return false;
        }
        return true;
    }

    // Parse an argument with a single expected value - without default (can be treated as 'must have')
    // Must be called explicitly like: 'TryParse<int>(...)' as C++ can't/won't deduce type-specification based on the return
    template<typename TValue>
    [[nodiscard]]
    std::optional<TValue> TryParse(const std::string &shortParamName, const std::string &longParamName = {}) const {
        return TryParse<TValue>({}, shortParamName, longParamName);
    }

    // Same as above but for r-value ref's
    template<typename TValue>
    [[nodiscard]]
    std::optional<TValue> TryParse(const TValue &&defaultValue, const std::string &shortParamName, const std::string &longParamName = {}) const {
        return TryParse(defaultValue, shortParamName, longParamName);
    }

    // Parse an argument with a single expected value - using a default value if arument is not present...
    // type deduction based on the default value...
    template<typename TValue>
    [[nodiscard]]
    std::optional<TValue> TryParse(const TValue &defaultValue, const std::string &shortParamName, const std::string &longParamName = {}) const {
        // pre-populate with default value, will be set to parsed/converted value by Lamda if everything works out...
        TValue result = {defaultValue};
        auto valueFunc = [&result, this](int idxArgValue) -> kParseResult {
                const std::string_view argValue = args[idxArgValue];
                auto res = convert_to<TValue>(argValue);
                if (!res.has_value()) {
                    return kParseResult::ErrArgTypeError;
                }
                result = *res;
                return kParseResult::Ok;
        };

        auto res = TryParseInternal(true, valueFunc, shortParamName, longParamName);
        if ((res == kParseResult::Ok) || (res == kParseResult::OkNotPresent)) {
            return result;
        }

        return {};
    }

    // Parse an argument with an array as expected value
    template<typename TValue>
    [[nodiscard]]
    int TryParse(std::vector<TValue> &outValues, const std::string &shortParamName, const std::string &longParamName = {}) const {
        int nCopied = 0;

        // lambda to convert an array of TValue
        // like: '--input_files <f1> <f2> <f3> <f4>
        auto valueFunc = [&outValues, &nCopied, this](int idxArgValue) -> kParseResult {

            while(true) {
                // FIXME: Split this string in ',' as an optional...
                auto v = convert_to<TValue>(args[idxArgValue]);
                if (!v.has_value()) {
                    return kParseResult::ErrArgTypeError;
                }
                outValues.push_back(*v);
                if ((idxArgValue + 1) >= args.size()) break;
                if (args[idxArgValue+1][0] == '-') break;
                ++idxArgValue;
                ++nCopied;
            }
            return kParseResult::Ok;
        };

        auto res = TryParseInternal(true, valueFunc, shortParamName, longParamName);
        if ((res == kParseResult::Ok) || (res == kParseResult::OkNotPresent)) {
            return nCopied;
        }

        return 0;
    }

    [[nodiscard]]
    int CountPresence(const std::string &shortParamName, const std::string &longParamName = {}) const {
        // We need a specialized version here...
        int nFound = 0;
        for(size_t i=0;i<args.size();++i) {
            std::string_view arg = args[i];
            if (!IsValidArgument(arg)) {
                continue;
            }

            // simple check if we have a single parameter ('-a' or '--name') with/without arguments
            if (arg == longParamName) {
                nFound++;
            } else {
                // If this is a 'long' parameter - just skip it...
                if ((arg.length() > 1) && (arg[0] == '-') && (arg[1] == '-')) continue;
                // now check every letter in the argument and if they are present in our short parameter name
                for (size_t j = 1; j < arg.length(); j++) {
                    if (shortParamName.find(arg[j]) != std::string::npos) {
                        nFound++;
                    }
                }
            }
        }
        return nFound;
    }

    template<typename TValue>
    [[nodiscard]]
    int CopyEndArgs(std::vector<TValue> &outValues) const {

        auto it = args.end()-1;
        while((*it[0] != '-') && (it != args.begin())) --it;
        // got all the way to prgname, so lets advance...
        ++it;
        // are we at the end => nothing but prgname was supplied
        // or is the argument now a '-<name>' which means there was no end-of-cmdline parameters passed
        if ((it == args.end()) || (*it[0] == '-')) {
            return 0;
        }

        int nValues = 0;
        while(it != args.end()) {
            auto res = convert_to<TValue>(*it);
            if (!res.has_value()) {
                return -1;
            }
            outValues.push_back(*res);
            nValues++;
            ++it;
        }

        return nValues;
    }

    int CopyAllAfter(std::vector<std::string> &outValues, const std::string &param) const {
        auto itParam = std::find_if(args.begin(), args.end(), [&](const std::string_view &arg) { return arg == param; });
        if (itParam == args.end()) {
            return -1;
        }
        ++itParam;
        while(itParam != args.end()) {
            outValues.push_back(*itParam);
            ++itParam;
        }
        return (int)outValues.size();
    }

    bool IsLastArgument(const std::string &shortParamName, const std::string &longParamName = {}) const {
        auto it = args.end()-1;

        while(it != args.begin()) {
            if ((*it[0] == '-') && ((*it == shortParamName) || (*it == longParamName))) {
                return true;
            }
            if (*it[0] == '-') {
                return false;
            }
            --it;
        }
        return false;
    }

protected:
    static bool IsValidArgument(const std::string_view &arg) {
        if (arg.empty() || arg[0] != '-') {
            return false;
        }
        return true;
    }
    // Returns
    //  false - indicates an error
    //  true  - no error
    template<typename TFunc>
    [[nodiscard]]
    kParseResult TryParseInternal(bool bHaveParam, TFunc cbParam, const std::string &shortParamName, const std::string &longParamName = {}) const {
        for(size_t i=0;i<args.size();++i) {
            std::string_view arg = args[i];
            if (!IsValidArgument(arg)) {
                continue;
            }

            // simple check if we have a single parameter ('-a' or '--name') with/without arguments
            if ((arg == shortParamName) || (arg == longParamName)) {
                // If we have params, make sure there are arguments left to support them...
                if (bHaveParam && (i + 1) >= args.size()) {
                    fprintf(stderr, "Argument missing for '%s' (check argc in CTOR)\n", shortParamName.c_str());
                    return kParseResult::ErrMissingArg;
                }

                // use callback to handle parameter
                return cbParam(++i);
            }

            // this is a long name - we do NOT check them for presence of short paramnames (used for flags)
            if (arg.starts_with("--")) continue;

            //
            // check if the parameter is embedded in a list of parameters (like '-b' in '-abc')
            // Note: We DO NOT allow arguments here, by design...
            //
            for(size_t j=1;j<arg.length();j++) {
                if (shortParamName.find(arg[j]) != std::string::npos) {
                    // use callback to handle parameter, we could also simply return 'Ok' here
                    // We can also simply return 'Ok' here
                    return cbParam(++i);
                }
            }

        }
        return kParseResult::OkNotPresent;
    }

    template<typename T>
    [[nodiscard]]
    std::optional<T> convert_to(std::string_view sv) const {
        //T value;

        if constexpr (std::is_same_v<T, std::string>) {
            return std::string{sv};
        }
        else if constexpr (std::is_same_v<T, std::string_view>) {
            return sv;
        }
        else if constexpr (std::is_same_v<T, bool>) {
            if (sv == "1" || sv == "true"  || sv == "TRUE")  return true;
            if (sv == "0" || sv == "false" || sv == "FALSE") return false;
            return std::nullopt;
        }
        else if constexpr (std::is_arithmetic_v<T>) {
            T out{};
            auto first = sv.data();
            auto last  = sv.data() + sv.size();

            // This is only available on macos from 26.0 and onwards...

            // std::from_chars handles all integers and floating point (C++17+)
//            auto [ptr, ec] = std::from_chars(first, last, out);
//            if (ec == std::errc{} && ptr == last)
//                return out;

            auto ec = parse_number(first, last, out);
            if (ec == std::errc{}) {
                return out;
            }

            return std::nullopt;
        }
        else {
            // Unsupported type; static assert provides helpful compile-time error
            static_assert(!sizeof(T*), "convert_to<T>: Unsupported type");
        }
    }

    template<typename T>
    std::errc parse_number(const char* first, const char* last, T& out) const {
#if defined(__cpp_lib_to_chars)
        // Use real from_chars
        auto r = std::from_chars(first, last, out);
        return r.ec;
#else
        // Fallback: use strtol / strtod depending on T
        if constexpr (std::is_integral_v<T>) {
            errno = 0;
            char* end;
            long long v = std::strtoll(first, &end, 10);
            if (errno != 0 || end != last)
                return std::errc::invalid_argument;
            out = static_cast<T>(v);
            return {};
        } else if constexpr (std::is_floating_point_v<T>) {
            errno = 0;
            char* end;
            double v = std::strtod(first, &end);
            if (errno != 0 || end != last)
                return std::errc::invalid_argument;
            out = static_cast<T>(v);
            return {};
        }
#endif
    }
private:
    std::span<const char *> args;
};

#endif
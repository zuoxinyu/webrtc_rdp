#pragma once

#include <string>

struct Protocol {
    static constexpr std::string kSignIn = "/sign_in";
    static constexpr std::string kSignOut = "/sign_out";
    static constexpr std::string kSendTo = "/send";
    static constexpr std::string kWait = "/wait";
};

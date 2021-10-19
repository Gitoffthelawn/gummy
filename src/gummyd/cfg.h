/**
 * Copyright (C) Francesco Fusco. All rights reserved.
 * License: https://github.com/Fushko/gammy#license
 */

#ifndef CFG_H
#define CFG_H

#include "json.hpp"

using  json = nlohmann::json;
extern json cfg;

namespace config {
std::string getPath();
void read();
void write();
}

#endif // CFG_H

#include <iostream>
#include "LogManager.h"
#include "config.h"

//#define DMSG(x) std::cerr << "    DEBUG: [" << __PRETTY_FUNCTION__ << ":" << __LINE__ << "] " << x << std::endl;
#define DMSG(x) pga::LogManager::logger().debug() << x << std::endl;


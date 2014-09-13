/*===============================================================
*   
*    File Name   : main.cc
*    Author      : Bert Young
*    Date        : 2014.08.13
*    Description : 
*    Tencent.co
================================================================*/
#include "./Logger.h"
#include <iostream>

int main()
{
    Logger  log;
    if (!log.Init(Logger::logERROR,  Logger::logALL))
    {
        std::cerr << "init failed\n";
        return -1;
    }

    LOG_INF(log) << "my age is " << 30 << Logger::endl;
    LOG_ERR(log) << "my age is " << 30 << Logger::endl;

    return 0;
}

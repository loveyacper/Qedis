#include "QStore.h"
#include "QClient.h"
#include "Log/Logger.h"
#include <iostream>
#include <cassert>

using namespace std;


QError  select(const vector<QString>& params, UnboundedBuffer& reply)
{
    assert (params[0] == "select");

    int newDb = atoi(params[1].c_str());
    bool succ = QClient::Current()->SelectDB(newDb);
    assert(succ);
    
    FormatOK(reply);
    return   QError_ok;
}


#include "QModuleInit.h"
#include "QCommand.h"
#include <algorithm>

extern "C"
qedis::QError ldel(const std::vector<qedis::QString>& params, qedis::UnboundedBuffer* reply);

extern "C"
qedis::QError hgets(const std::vector<qedis::QString>& params, qedis::UnboundedBuffer* reply);

extern "C"
qedis::QError skeys(const std::vector<qedis::QString>& params, qedis::UnboundedBuffer* reply);

bool QedisModule_OnLoad()
{
    printf("enter %s\n", __FUNCTION__);
    using namespace qedis;

    // register list ldel command
    static QCommandInfo ldelinfo;
    ldelinfo.cmd = "ldel";
    ldelinfo.attr = QAttr_write;
    ldelinfo.params = 3;
    ldelinfo.handler = &ldel;

    if (!QCommandTable::AddCommand("ldel", &ldelinfo))
        return false;

    // register hash hgets command
    static QCommandInfo hgetsinfo;
    hgetsinfo.cmd = "hgets";
    hgetsinfo.attr = QAttr_read;
    hgetsinfo.params = 3;
    hgetsinfo.handler = &hgets;

    if (!QCommandTable::AddCommand("hgets", &hgetsinfo))
    {
        QCommandTable::DelCommand("ldel");
        return false;
    }

    // register set skeys command
    static QCommandInfo skeysinfo;
    skeysinfo.cmd = "skeys";
    skeysinfo.attr = QAttr_read;
    skeysinfo.params = 3;
    skeysinfo.handler = &skeys;

    if (!QCommandTable::AddCommand("skeys", &skeysinfo))
    {
        QCommandTable::DelCommand("hgets");
        QCommandTable::DelCommand("ldel");
        return false;
    }
    
    printf("exit %s\n", __FUNCTION__);
    return true;
}


void QedisModule_OnUnLoad()
{
    printf("enter %s\n", __FUNCTION__);
    qedis::QCommandTable::DelCommand("skeys");
    qedis::QCommandTable::DelCommand("hgets");
    qedis::QCommandTable::DelCommand("ldel");
    printf("exit %s\n", __FUNCTION__);
}
